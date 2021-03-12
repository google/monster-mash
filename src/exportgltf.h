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

void exportStart(tinygltf::Model &m, const Imguc &textureImg);
void exportStop(tinygltf::Model &m, const std::string &outFn, bool writeBinary);
void exportFullModel(const MatrixXfR &V, const MatrixXfR &N,
                     const MatrixXusR &F, const MatrixXfR &TC,
                     const int nFrames, const int FPS, tinygltf::Model &m);
void exportMorphTarget(const MatrixXfR &V, const MatrixXfR &N, const int frame, const int nFrames,
                       const bool hasTexture, tinygltf::Model &m);

}  // namespace exportgltf

#endif  // EXPORTGLTF_H
