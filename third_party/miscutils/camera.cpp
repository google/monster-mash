// Copyright (c) 2018 Marek Dvoroznak
// Licensed under the MIT License.

#include "camera.h"
#include "macros.h"
#include <igl/frustum.h>
#include <igl/ortho.h>

using namespace Eigen;

static Matrix4d createFrustumMatrix(bool ortho, float width, float height, float near, float far)
{
  float l = -0.5*width; // left
  float r = 0.5*width; // right
  float b = -0.5*height; // bottom
  float t = 0.5*height; // top
  Matrix4d P;
  if (ortho) igl::ortho(l, r, b, t, near, far, P);
  else igl::frustum(l, r, b, t, near, far, P);
  return P;
}

void buildCameraMatrices(bool ortho, int viewportW, int viewportH, float near, float far, float rotVerDeg, float rotHorDeg, const Eigen::Vector2d &shiftImageSpace, const Eigen::Vector3d &centroid, Eigen::MatrixXd &P, Eigen::MatrixXd &M)
{
  float width, height;
  if (!ortho) {
    width = viewportW;
    height = viewportH;
    P = createFrustumMatrix(false, width, height, near, far);
  } else {
    width = viewportW;
    height = viewportH;
    P = createFrustumMatrix(true, width, height, near, far);
  }
  P = P * Affine3d(Translation3d(shiftImageSpace(0), shiftImageSpace(1), 0)).matrix();
  M = Affine3d( // from bottom to top
               Translation3d(0, 0, -(near+0.5*(far-near))) // translate off of the camera center in depth
               * AngleAxis<double>(-rotVerDeg/180.0*M_PI, Vector3d(1,0,0)) // rotate
               * AngleAxis<double>(rotHorDeg/180.0*M_PI, Vector3d(0,1,0)) // rotate
               * Translation3d(-centroid(0), -centroid(1), -centroid(2)) // translate to the centroid
               * Scaling(1.,1.,-1.) // view it from front, not back
               ).matrix();
}
