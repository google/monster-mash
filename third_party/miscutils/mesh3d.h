// Copyright (c) 2018 Marek Dvoroznak
// Licensed under the MIT License.

#ifndef MESH3D_H
#define MESH3D_H

#include <Eigen/Dense>

class Mesh3D
{
public:
  Mesh3D();
  virtual ~Mesh3D();

  void loadMesh(const std::string &fn);
  void createFromMesh(const Eigen::MatrixXd &V, const Eigen::MatrixXi &F);

  static int getMeshPoint(const Eigen::MatrixXd &V, double x, double y, double radius, double depth = 0.0, bool considerDepth = true);
  static int getMeshPoint(const Eigen::MatrixXd &V, double x, double y, double planarRadius, bool highest);
  static int getMeshFace(const Eigen::MatrixXd &V, const Eigen::MatrixXi &F, double x, double y, bool highestZ, bool reverseDirection);
  static int getMeshFace(const Eigen::MatrixXd &V, const Eigen::MatrixXi &F, double x, double y, bool highestZ, bool reverseDirection, double &w1, double &w2, double &w3);

  int getNumPoints() const;
  int getNumFaces() const;
  void reset(const Eigen::MatrixXd &V);

  Eigen::MatrixXd VRest, VCurr;
  Eigen::MatrixXi F;
};

#endif // MESH3D_H
