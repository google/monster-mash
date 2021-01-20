// Copyright 2020-2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "def3dsdl.h"

using namespace Eigen;

void drawControlPoint(const Def3D::CP &cp, MyPainter &painter, int size,
                      const Eigen::Matrix4d &M) {
  const Vector3d &p = (M * cp.pos.homogeneous()).hnormalized();
  painter.setColor(0, 0, 0, 255);
  painter.filledEllipse(p(0), p(1), size, size);
  painter.setColor(255, 0, 0, 255);
  painter.filledEllipse(p(0), p(1), size - 2, size - 2);
}

void drawControlPoint(const Eigen::VectorXd &q, MyPainter &painter, int size,
                      const Eigen::Matrix4d &M, int thickness,
                      const Cu &colorBg, const Cu &colorFg) {
  const Vector3d &p = (M * q.homogeneous()).hnormalized();
  painter.setColor(colorBg);
  painter.filledEllipse(p(0), p(1), size, size);
  painter.setColor(colorFg);
  painter.filledEllipse(p(0), p(1), size - thickness, size - thickness);
}

void drawControlPoints(const Def3D &def, MyPainter &painter, int size,
                       const Eigen::Matrix4d &M, int thickness,
                       const Cu &colorBg, const Cu &colorFg) {
  auto &cps = def.getCPs();
  for (const auto &it : cps) {
    drawControlPoint(it.second->pos, painter, size, M, thickness, colorBg,
                     colorFg);
  }
}
