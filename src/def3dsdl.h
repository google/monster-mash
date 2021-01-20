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

#ifndef DEF3DSDL_H
#define DEF3DSDL_H

#include <miscutils/def3d.h>

#include <Eigen/Dense>

#include "mypainter.h"

void drawControlPoint(const Def3D::CP &cp, MyPainter &painter, int size,
                      const Eigen::Matrix4d &M);
void drawControlPoint(const Eigen::VectorXd &q, MyPainter &painter, int size,
                      const Eigen::Matrix4d &M, int thickness = 1,
                      const Cu &colorBg = Cu{0, 0, 0, 255},
                      const Cu &colorFg = Cu{255, 0, 0, 255});
void drawControlPoints(const Def3D &def, MyPainter &painter, int size,
                       const Eigen::Matrix4d &M, int thickness = 1,
                       const Cu &colorBg = Cu{0, 0, 0, 255},
                       const Cu &colorFg = Cu{255, 0, 0, 255});

#endif  // DEF3DSDL_H
