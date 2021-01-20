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

#include "cpanim.h"

#include <iostream>

#include "macros.h"

using namespace Eigen;
using namespace std;

CPAnim::CPAnim() {}

CPAnim::CPAnim(unsigned int length, const Eigen::Vector3d &p) {
  keyposes.resize(length, Keypose{p, true});
}

CPAnim::CPAnim(const std::vector<Keypose> &keyposes) {
  this->keyposes = keyposes;
}

void CPAnim::record(unsigned int t, const Keypose &k) {
  assert(t < getLength());
  keyposes[t] = k;
  lastT = t;
}

void CPAnim::record(unsigned int t, const Eigen::Vector3d &p,
                    const int timestamp) {
  record(t, Keypose{p, timestamp, false, true, true});
}

void CPAnim::record(const Keypose &k) {
  keyposes.push_back(k);
  lastT = keyposes.size() - 1;
}

void CPAnim::record(const Eigen::Vector3d &p, const int timestamp) {
  record(Keypose{p, timestamp, false, true, true});
}

Eigen::Vector3d CPAnim::replay(double t) {
  double speedup = temporalScalingFactor;
  const int length = getLength();
  if (length == 0) {
    return Vector3d(0, 0, 0);
  }
  if (syncLength > 0) speedup *= length / static_cast<double>(syncLength);
  lastT = t;
  t = t * speedup + offset;
  lastTTransformed = t;

  int i = static_cast<int>(floor(t));
  double f = t - i;
  i = i % length;
  if (i < 0) i = length + i;

  int t0 = i, t1;
  if (i == length - 1) {
    t1 = 0;  // first and last
  } else {
    t1 = i + 1;
  }
  assert(t0 >= 0 && t1 >= 0 && t0 < length && t1 < length);

  const Vector3d p = keyposes[t0].p * (1.0 - f) + keyposes[t1].p * f;
  return (T * p.homogeneous()).hnormalized();
}

void CPAnim::syncSetLength(int length) { syncLength = length; }

Eigen::Vector3d CPAnim::replay() {
  const Vector3d &p = replay(lastT);
  lastT++;
  if (lastT >= keyposes.size()) lastT = 0;
  return p;
}

CPAnim::Keypose &CPAnim::peekk(int t) {
  assert(t >= 0 && t < getLength());
  return keyposes[t];
}

Eigen::Vector3d CPAnim::peek(double t) { return replay(t); }

Eigen::Vector3d CPAnim::peek() { return replay(lastT); }

void CPAnim::restart() { lastT = 0; }

int CPAnim::getLength() const { return keyposes.size(); }

void CPAnim::setLength(int length) { keyposes.resize(length, keyposes.back()); }

double CPAnim::getTransformedLength() const {
  double speedup = temporalScalingFactor;
  const int length = getLength();
  if (syncLength > 0) speedup *= length / static_cast<double>(syncLength);
  return length * speedup;
}

bool CPAnim::isActive() const { return active; }

bool CPAnim::isAtBeginning() const { return lastT == 0; }

void CPAnim::drawTrajectory(MyPainter &painter, const Eigen::Matrix4d &M,
                            double beginningThicknessMult) {
  const Matrix4d &MT = M * T;
  fora(i, 0, keyposes.size()) {
    const auto &k0 = keyposes[i == 0 ? keyposes.size() - 1 : (i - 1)];
    const auto &k1 = keyposes[i];
    if (!k0.display || !k1.display) continue;
    const Vector3d p0 = (MT * k0.p.homogeneous()).hnormalized();
    const Vector3d p1 = (MT * k1.p.homogeneous()).hnormalized();
    painter.drawLine(p0(0), p0(1), p1(0), p1(1));
    if (i == 0) {
      int thickness = painter.getCurrentThickness();
      painter.filledEllipse(p1(0), p1(1), beginningThicknessMult * thickness,
                            beginningThicknessMult * thickness);
    }
  }
}

void CPAnim::drawKeyframes(MyPainter &painter, int r,
                           const Eigen::Matrix4d &M) {
  const Matrix4d &MT = M * T;
  for (const auto &k : keyposes) {
    if (!k.display) continue;
    const Vector3d p = (MT * k.p.homogeneous()).hnormalized();
    painter.drawEllipse(p(0), p(1), r, r);
  }
}

void CPAnim::setTransform(const Eigen::Matrix4d &T) { this->T = T; }

Eigen::MatrixXd &CPAnim::getTransform() { return T; }

void CPAnim::applyTransform() {
  for (auto &k : keyposes) {
    k.p = (T * k.p.homogeneous()).hnormalized();
  }
  T.setIdentity();
}

Eigen::Vector3d CPAnim::getCentroid() const {
  Vector3d centroid(0, 0, 0);
  for (const auto &k : keyposes) centroid += k.p;
  return centroid / keyposes.size();
}

ostream &operator<<(ostream &out, const CPAnim &cpAnim) {
  const auto &keyposes = cpAnim.keyposes;
  if (cpAnim.getLength() > 0) {
    bool have_timestamps = keyposes[0].have_timestamp;
    out << "speedup " << cpAnim.getTemporalScalingFactor() << endl;
    out << "offset " << cpAnim.getOffset() << endl;
    out << "have_timestamps " << (have_timestamps ? 1 : 0) << endl;
    out << static_cast<int>(keyposes.size()) << endl;
    forlist(i, keyposes) {
      out << keyposes[i].p.transpose();
      if (have_timestamps) out << " " << keyposes[i].timestamp;
      out << endl;
    }
  }
  return out;
}

istream &operator>>(istream &in, CPAnim &cpAnim) {
  string str;
  double d;
  bool have_timestamps = false;
  while (!in.eof()) {
    long pos = in.tellg();
    in >> str;
    if (str == "speedup") {
      in >> d;
      cpAnim.setTemporalScalingFactor(d);
    } else if (str == "offset") {
      in >> d;
      cpAnim.setOffset(d);
    } else if (str == "have_timestamps") {
      int i;
      in >> i;
      if (i == 1) have_timestamps = true;
    } else {
      in.seekg(pos);
      break;
    }
  }

  int num;
  in >> num;
  fora(i, 0, num) {
    double x, y, z;
    in >> x >> y >> z;
    int timestamp = -1;
    if (have_timestamps) {
      in >> timestamp;
    }
    cpAnim.record(CPAnim::Keypose{Vector3d(x, y, z), timestamp, false, true,
                                  have_timestamps});
  }
  return in;
}

void CPAnim::setOffset(double val) { offset = val; }

double CPAnim::getOffset() const { return offset; }

void CPAnim::setTemporalScalingFactor(double val) {
  temporalScalingFactor = val;
}

double CPAnim::getTemporalScalingFactor() const {
  return temporalScalingFactor;
}

void CPAnim::setAll(bool empty, bool display) {
  for (auto &k : keyposes) {
    k.empty = empty;
    k.display = display;
  }
}

const std::vector<CPAnim::Keypose> &CPAnim::getKeyposes() { return keyposes; }

void CPAnim::performSmoothing(int tFrom, int tTo, int iterations) {
  const int length = getLength();
  if (length == 0) return;
  fora(it, 0, iterations) {
    fora(i, tFrom, tTo + 1) {
      int i1 = (i - 1) % length;
      if (i1 < 0) i1 = length + i1;
      int i2 = (i1 + 1) % length;  // always positive
      int i3 = (i1 + 2) % length;  // always positive
      auto &k1 = keyposes[i1];
      auto &k2 = keyposes[i2];
      auto &k3 = keyposes[i3];
      k2.p = (k1.p + k2.p + k3.p) / 3.0;
    }
  }
}
