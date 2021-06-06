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

#ifndef EXPORTGLTF_H
#define EXPORTGLTF_H

#include <image/image.h>
#include <tiny_gltf.h>

#include <Eigen/Core>
#include <string>

namespace exportgltf {

typedef Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>
    MatrixXfR;
typedef Eigen::Matrix<unsigned short, Eigen::Dynamic, Eigen::Dynamic,
                      Eigen::RowMajor>
    MatrixXusR;

class ExportGltf {
 public:
  void exportStart(const MatrixXfR &V, const MatrixXfR &N, const MatrixXusR &F,
                   const MatrixXfR &TC, const int nFrames, bool mtHasNormals,
                   const int FPS, const Imguc &textureImg);
  void exportStop(const std::string &outFn, bool writeBinary);
  void exportFullModel(const MatrixXfR &V, const MatrixXfR &N,
                       const MatrixXusR &F, const MatrixXfR &TC);
  void exportMorphTarget(const MatrixXfR &V, const MatrixXfR &N,
                         const int frame);

 private:
  size_t nBytesF = 0;
  size_t nBytesV = 0;
  size_t nBytesN = 0;
  size_t nBytesTC = 0;
  size_t nBytesBase = 0;
  int nWeights = 0;
  size_t nBytesWeights = 0;
  size_t nBytesTime = 0;
  size_t nBytesAnim = 0;
  size_t nBytesImage = 0;
  size_t nBytesMTTotal = 0;
  size_t nBytesMTCum = 0;
  bool mtHasNormals = false;
  int nFrames = 0;
  int FPS = 0;
  bool hasTexture = false;
  tinygltf::Model m;
};

}  // namespace exportgltf

#endif  // EXPORTGLTF_H
