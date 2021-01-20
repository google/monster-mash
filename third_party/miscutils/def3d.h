// Copyright (c) 2018 Marek Dvoroznak
// Licensed under the MIT License.

#ifndef DEF3D_H
#define DEF3D_H

#include "mesh3d.h"
#include <memory>
#include <limits>
#include <map>
#include <Eigen/Dense>
#include <miscutils/macros.h>

class Def3D
{
public:
  class CP // control points
  {
  public:
    Eigen::Vector3d pos, prevPos;
    bool fixed;
    double weight;
    int ptId = 0; // control point to mesh points correspondence

    CP(const Eigen::Vector3d &pos, bool fixed, double weight) : pos(pos), prevPos(pos), fixed(fixed), weight(weight) {}
    CP() : CP(Eigen::Vector3d(0,0,0), false, 1) {}
  };

  Def3D();
  virtual ~Def3D();

  void updatePtsAccordingToCPs(Mesh3D &inMesh);

  const CP& getCP(int index) const;
  CP& getCP(int index);
  const std::map<int, std::shared_ptr<CP> > &getCPs() const;
  int addCP(CP &cp);
  int getControlPoint(double x, double y, double radius, double depth, bool considerDepth, bool highestDepth, const Eigen::Matrix4d &M = Eigen::Matrix4d::Identity());
  std::vector<int> getControlPointsInsideRect(double x1, double y1, double x2, double y2, const Eigen::Matrix4d &M = Eigen::Matrix4d::Identity());
  bool addControlPointOnFace(const Eigen::MatrixXd &V, const Eigen::MatrixXi &F, double x, double y, double planarRadius, bool highest, bool reverseDirection, int &index, const Eigen::Matrix4d &M = Eigen::Matrix4d::Identity());
  bool addControlPointOnFace(const Eigen::MatrixXd &V, const Eigen::MatrixXi &F, double x, double y, double planarRadius, bool highest, bool reverseDirection);
  bool addControlPoint(const Eigen::MatrixXd &V, double x, double y, double planarRadius, bool highest, int &index, const Eigen::MatrixXd &M = Eigen::Matrix4d::Identity());
  bool addControlPoint(const Eigen::MatrixXd &V, double x, double y, double radius, double depth = 0.0, bool considerDepth = true);
  bool addControlPoint(const Eigen::MatrixXd &V, double x, double y, double radius, double depth, int &index, bool considerDepth = true);
  bool addControlPointExhaustive(const Eigen::MatrixXd &V, double x, double y, double radius, double depth, bool considerDepth, int &index);
  bool removeControlPoint(double x, double y, double radius, double depth = 0.0, bool considerDepth = true);
  bool removeControlPoint(int index);
  bool removeLastControlPoint();
  void removeControlPoints();
  long getCp2ptChangedNum() const;

  double cpDefaultWeight = 1.0; // default weight of control points for control points created by addControlPoint... functions
  bool cpDefaultFixed = true; // default behavior of control points for control points created by addControlPoint... functions

protected:
  std::map<int,std::shared_ptr<CP>> cps;
  long cpChangedNum = 0;
  int nextId = 0;
};

#endif // DEF3D_H
