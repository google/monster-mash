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

#ifndef CPANIM_H
#define CPANIM_H

#include <Eigen/Dense>
#include <iostream>
#include <vector>

#include "mypainter.h"

class CPAnim {
 public:
  struct Keypose {
    Eigen::Vector3d p;
    int timestamp;
    bool empty = true;
    bool display = false;
    bool have_timestamp = true;
  };

  CPAnim();
  CPAnim(unsigned int length, const Eigen::Vector3d &p);
  CPAnim(const std::vector<Keypose> &keyposes);

  void record(const Eigen::Vector3d &p, const int timestamp);
  void record(unsigned int t, const Eigen::Vector3d &p, const int timestamp);
  void record(const Keypose &k);
  void record(unsigned int t, const Keypose &k);
  Eigen::Vector3d replay(double t);
  Eigen::Vector3d replay();
  Eigen::Vector3d peek(double t);
  Eigen::Vector3d peek();
  Keypose &peekk(int t);
  void syncSetLength(int length);
  void restart();
  int getLength() const;
  void setLength(int length);
  double getTransformedLength() const;
  bool isActive() const;
  bool isAtBeginning() const;
  void drawTrajectory(MyPainter &painter,
                      const Eigen::Matrix4d &M = Eigen::Matrix4d::Identity(),
                      double beginningThicknessMult = 2);
  void drawKeyframes(MyPainter &painter, int r,
                     const Eigen::Matrix4d &M = Eigen::Matrix4d::Identity());
  void setTransform(const Eigen::Matrix4d &T);
  Eigen::MatrixXd &getTransform();
  void applyTransform();
  Eigen::Vector3d getCentroid() const;
  void setOffset(double val);
  double getOffset() const;
  void setTemporalScalingFactor(double val);
  double getTemporalScalingFactor() const;
  void setAll(bool empty, bool display);
  const std::vector<Keypose> &getKeyposes();
  void performSmoothing(int tFrom, int tTo, int iterations);

  friend std::ostream &operator<<(std::ostream &out, const CPAnim &cpanim);
  friend std::istream &operator>>(std::istream &in, CPAnim &cpanim);

  double lastT = 0;
  double lastTTransformed = 0;

 private:
  Eigen::MatrixXd T = Eigen::Matrix4d::Identity();
  std::vector<Keypose> keyposes;
  bool active = true;
  double offset = 0;
  int syncLength = 0;
  double temporalScalingFactor = 1;
};

#endif  // CPANIM_H
