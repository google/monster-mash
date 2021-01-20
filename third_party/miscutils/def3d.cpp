// Copyright (c) 2018 Marek Dvoroznak
// Licensed under the MIT License.

#include "def3d.h"
#include "mesh3d.h"
#include <unordered_set>
#include <iostream>
#include <vector>

using namespace Eigen;
using namespace std;

Def3D::Def3D()
{
}

Def3D::~Def3D()
{
}

void Def3D::updatePtsAccordingToCPs(Mesh3D &inMesh)
{
  for(const auto &it : cps) {
    const CP &cp = *it.second;
    inMesh.VCurr.row(cp.ptId) = cp.pos;
//      p.weight = cp.weight;
//      p.fixed = cp.fixed;
  }
}

const Def3D::CP& Def3D::getCP(int index) const
{
  const auto &it = cps.find(index);
  if (it == cps.end()) {
    throw(out_of_range("Def3D::getCP: index (" + to_string(index) + ") not found"));
  }
  return *it->second;
}

Def3D::CP& Def3D::getCP(int index)
{
  return const_cast<CP&>(static_cast<const Def3D&>(*this).getCP(index));
}

const std::map<int,std::shared_ptr<Def3D::CP>>& Def3D::getCPs() const
{
  return cps;
}

int Def3D::addCP(CP &cp)
{
  cps[nextId] = make_shared<CP>(cp);
  nextId++;
  cpChangedNum++;
  return nextId-1;
}

std::vector<int> Def3D::getControlPointsInsideRect(double x1, double y1, double x2, double y2, const Eigen::Matrix4d &M)
{
  vector<int> ids;
  for(const auto &it : cps) {
    const CP &cp = *it.second;
    const int cpId = it.first;
    const Vector3d p = (M * cp.pos.homogeneous()).hnormalized();
    if (p(0) >= x1 && p(1) >= y1 && p(0) <= x2 && p(1) <= y2) ids.push_back(cpId);
  }
  return ids;
}

int Def3D::getControlPoint(double x, double y, double radius, double depth, bool considerDepth, bool highestDepth, const Eigen::Matrix4d &M)
{
  int ind = -1;
  double extDepth = highestDepth ? -numeric_limits<double>::infinity() : numeric_limits<double>::infinity();
  double minDist = numeric_limits<double>::infinity();
  Vector3d pos(x,y,depth);
  for(const auto &it : cps) {
    CP &cp = *it.second;
    const int cpId = it.first;
    const Vector3d p = (M * cp.pos.homogeneous()).hnormalized();
    double d;
    if (considerDepth) d = (pos-p).norm();
    else d = (pos.head(2)-p.head(2)).norm();
    double depth = p(2);
    if (d <= radius && d < minDist && (highestDepth ? depth>extDepth : depth<extDepth)) {
      minDist = d;
      extDepth = depth;
      ind = cpId;
    }
  }
  return ind;
}

bool Def3D::addControlPointOnFace(const MatrixXd &V, const MatrixXi &F, double x, double y, double planarRadius, bool highest, bool reverseDirection)
{
  int index;
  return addControlPointOnFace(V, F, x, y, planarRadius, highest, reverseDirection, index);
}

bool Def3D::addControlPointOnFace(const MatrixXd &V, const MatrixXi &F, double x, double y, double planarRadius, bool highest, bool reverseDirection, int &index, const Eigen::Matrix4d &M)
{
  int ind = getControlPoint(x, y, planarRadius, 0, false, highest, M);
  if (ind != -1) {
    index = ind;
    return false;
  }

  const MatrixXd VProj = (V.rowwise().homogeneous() * M.transpose()).rowwise().hnormalized();
  int faceId = Mesh3D::getMeshFace(VProj, F, x, y, highest, reverseDirection);
  if (faceId == -1) {
    index = -1;
    return false;
  }

  const VectorXi &facePts = F.row(faceId);
  // choose the nearest point of the face
  double min = numeric_limits<double>::infinity();
  Vector2d pos(x,y);
  int ptId = -1;
  fora(i, 0, facePts.size()) {
    const Vector3d &p = VProj.row(facePts(i));
    double d = (p.head(2)-pos).norm();
    if (d < min) {
      min = d;
      ptId = facePts[i];
    }
  }
  ind = ptId;

  // check if such control point is associated to a mesh point
  for(const auto &it : cps) {
    const int cpId = it.first;
    const CP &cp = *it.second;
    if (cp.ptId == ind) {
      index = cpId;
      return false;
    }
  }
  double z = VProj(ind,2);
  const Vector3d unproj = (M.inverse() * Vector3d(x,y,z).homogeneous()).hnormalized();
  CP cp(unproj, cpDefaultFixed, cpDefaultWeight);
  cp.prevPos = cp.pos;
  cp.ptId = ind;
  index = addCP(cp);
  return true;
}

bool Def3D::addControlPoint(const MatrixXd &V, double x, double y, double planarRadius, bool highest, int &index, const MatrixXd &M)
{
  int ind = getControlPoint(x, y, planarRadius, 0, false, highest);
  if (ind != -1) {
    index = ind;
    return false;
  }

  const MatrixXd VProj = (V.rowwise().homogeneous() * M.transpose()).rowwise().hnormalized();
  ind = Mesh3D::getMeshPoint(VProj, x, y, planarRadius, highest);
  if (ind == -1) {
    index = -1;
    return false;
  }

  double z = VProj(ind,2);
  const Vector3d unproj = (M.inverse() * Vector3d(x,y,z).homogeneous()).hnormalized();
  CP cp(unproj, cpDefaultFixed, cpDefaultWeight);
  cp.ptId = ind;
  index = addCP(cp);
  return true;
}

// Adds a control point to a mesh point that is the nearest to a point with [x,y,depth] coordinate
// within a radius only if there is not a control point associated to the mesh point already.
// Returns true if new control point was added and also it's index.
// Returns false if there is already a control point and also index of the control point.
// Returns false if there is not a mesh point within the radius and also index -1 in this case.
bool Def3D::addControlPoint(const MatrixXd &V, double x, double y, double radius, double depth, int &index, bool considerDepth)
{
  int ind = getControlPoint(x, y, radius, depth, considerDepth, false);
  if (ind != -1) {
    index = ind;
    return false;
  }
  ind = Mesh3D::getMeshPoint(V, x, y, radius, depth, considerDepth);
  if (ind == -1) {
    index = -1;
    return false;
  }
  CP cp(Vector3d(x,y,depth), cpDefaultFixed, cpDefaultWeight);
  cp.ptId = ind;
  index = addCP(cp);
  return true;
}

// Adds a control point to a mesh point that is the nearest to a point with [x,y,depth] coordinate
// within a radius and that is at the same time also not associated with some existing control point.
// It search the whole space around specified point and tries to find a "free" mesh point that is
// not associated with a control point.
// Returns true if new control point was added and also it's index.
// Returns false if there is already a control point and also index of the control point.
// Returns false if there is not a mesh point within the radius and also index -1 in this case.
bool Def3D::addControlPointExhaustive(const Eigen::MatrixXd &V, double x, double y, double radius, double depth, bool considerDepth, int &index)
{
  const int nPts = V.rows();
  const Vector3d pos(x,y,depth);

  // find the nearest mesh point that is not associated with a control point
  int bestInd = -1;
  int occupiedInd = -1;
  unordered_set<int> occupiedInds;
  for(const auto &it : cps) {
    int ptInd = it.second->ptId;
    occupiedInds.insert(ptInd);
  }
  double min = numeric_limits<double>::infinity();
  fora(ind, 0, nPts) {
    const Vector3d &p = V.row(ind);
    double d;
    const Vector3d dvec = p-pos;
    if (considerDepth) d = dvec.norm();
    else d = dvec.head(2).norm();
    if (d <= radius && d < min) {
      // test if the mesh point is associated with a control point
      const auto &it = occupiedInds.find(ind);
      if (it == occupiedInds.end()) {
        // this is the nearest point that is not associated
        bestInd = ind;
        min = d;
      } else {
        occupiedInd = *it;
      }
    }
  }

  if (bestInd != -1) {
    CP cp(Vector3d(x,y,depth), cpDefaultFixed, cpDefaultWeight);
    cp.ptId = bestInd;
    index = addCP(cp);
    return true;
  }

  index = occupiedInd;
  return false;
}

bool Def3D::removeControlPoint(int index)
{
  const auto &it = cps.find(index);
  if (it == cps.end()) return false;
  cps.erase(it);
  cpChangedNum++;
  return true;
}

bool Def3D::removeControlPoint(double x, double y, double radius, double depth, bool considerDepth)
{
  int ind = getControlPoint(x, y, radius, depth, considerDepth, false);
  return removeControlPoint(ind);
}

bool Def3D::removeLastControlPoint()
{
  return removeControlPoint(nextId-1);
}

void Def3D::removeControlPoints()
{
  cps.clear();
  cpChangedNum++;
}

long Def3D::getCp2ptChangedNum() const
{
  return cpChangedNum;
}
