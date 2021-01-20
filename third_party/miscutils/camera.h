// Copyright (c) 2018 Marek Dvoroznak
// Licensed under the MIT License.

#ifndef CAMERA_H
#define CAMERA_H

#include <Eigen/Dense>

void buildCameraMatrices(bool ortho, int viewportW, int viewportH, float near, float far, float rotVerDeg, float rotHorDeg, const Eigen::Vector2d &shiftImageSpace, const Eigen::Vector3d &centroid, Eigen::MatrixXd &P, Eigen::MatrixXd &M);

#endif // CAMERA_H
