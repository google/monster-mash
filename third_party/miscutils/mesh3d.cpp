// Copyright (c) 2018 Marek Dvoroznak
// Licensed under the MIT License.

#include "mesh3d.h"
#include <miscutils/macros.h>
#include <igl/readOBJ.h>

using namespace std;
using namespace Eigen;

Mesh3D::Mesh3D()
{
}

Mesh3D::~Mesh3D()
{
}

void Mesh3D::createFromMesh(const Eigen::MatrixXd &V, const Eigen::MatrixXi &F)
{
  VRest = VCurr = V;
  this->F = F;
}

void Mesh3D::loadMesh(const std::string &fn)
{
  using namespace igl;
  using namespace Eigen;
  MatrixXd V;
  MatrixXi F;
  readOBJ(fn, V, F);
  createFromMesh(V, F);
}

// Works for triangular faces only.
int Mesh3D::getMeshFace(const MatrixXd &V, const MatrixXi &F, double x, double y, bool highestZ, bool reverseDirection, double &w1, double &w2, double &w3)
{
  auto edgeFunction = [](const Vector3d &a, const Vector3d &b, const Vector3d &c)->double
  { return (c(0) - a(0)) * (b(1) - a(1)) - (c(1) - a(1)) * (b(0) - a(0)); };

  Vector3d pos(x,y,0);
  double extZ = highestZ ? -numeric_limits<double>::infinity() : numeric_limits<double>::infinity();
  int ind = -1;
  vector<double*> ws{&w1, &w2, &w3};
  fora(i, 0, F.rows()) {
    const VectorXi &f = F.row(i);
    bool inside = true;
    double area = 0;
    double z = 0;
    fora(j, 0, f.size()) {
      int ptIdFrom = f(j);
      int ptIdTo = f((j+1)%f.size());
      int ptIdOpposite = f((j+2)%f.size()); // only for triangular faces!
      const Vector3d &pFrom = V.row(ptIdFrom);
      const Vector3d &pTo = V.row(ptIdTo);
      const Vector3d &pOpposite = V.row(ptIdOpposite);
      double &w = *ws[j];
      w = edgeFunction(pFrom, pTo, pos);
      if (reverseDirection) w *= -1;
      if (w < 0) {
        inside = false;
        break;
      }
      z += w * pOpposite(2);
      area += w;
    }

    if (inside) {
      z /= area;
      if (highestZ ? z>extZ : z<extZ) {
        extZ = z;
        ind = i;
      }
    }
  }
  return ind;
}

// beware: probably works for triangular faces only
int Mesh3D::getMeshFace(const MatrixXd &V, const MatrixXi &F, double x, double y, bool highestZ, bool reverseDirection)
{
  double w1, w2, w3;
  return getMeshFace(V, F, x, y, highestZ, reverseDirection, w1, w2, w3);
}

// Find a point on the mesh of which planar projection is nearest to the specified point [x,y] while also being
// in the specified radius and with highest or lowest z-coordinate (i.e. nearest/farthest from the plane)
int Mesh3D::getMeshPoint(const MatrixXd &V, double x, double y, double planarRadius, bool highest)
{
  using namespace std;
  using namespace Eigen;

  int ind = -1;
  double min = numeric_limits<double>::infinity();
  double extZ = highest ? -numeric_limits<double>::infinity() : numeric_limits<double>::infinity();
  const Vector2d pos(x,y);
  fora(i, 0, V.rows()) {
    const Vector3d &pt = V.row(i);
    const Vector2d p(pt(0),pt(1));
    double d = (pos-p).norm();
    double z = pt(2);
    if (d <= planarRadius && d < min && (highest ? z>extZ : z<extZ)) {
      min = d;
      extZ = z;
      ind = i;
    }
  }
  return ind;
}

int Mesh3D::getMeshPoint(const MatrixXd &V, double x, double y, double radius, double depth, bool considerDepth)
{
  using namespace std;
  using namespace Eigen;

  int ind = -1;
  double min = numeric_limits<double>::infinity();
  Vector3d pos(x,y,depth);
  fora(i, 0, V.rows()) {
    const Vector3d &pt = V.row(i);
    Vector3d p(pt(0),pt(1),pt(2));
    double d;
    if (considerDepth) d = (pos-p).norm();
    else d = (pos.head(2)-p.head(2)).norm();
    if (d <= radius && d < min) {
      min = d;
      ind = i;
    }
  }
  return ind;
}

int Mesh3D::getNumPoints() const
{
  return VCurr.rows();
}

int Mesh3D::getNumFaces() const
{
  return F.rows();
}

void Mesh3D::reset(const Eigen::MatrixXd &V)
{
  VRest = VCurr = V;
}
