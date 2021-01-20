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

#ifndef DEPTHINFALGS_H
#define DEPTHINFALGS_H

#include <image/image.h>
#include <Eigen/Core>
#include <list>
#include <map>
#include <vector>

void eliminateCycles(
    const std::map<int, std::list<std::pair<int, float>>> &Eout,
    const std::vector<int> &regionsIdsUniq,
    std::map<int, std::list<std::pair<int, float>>> &Eout2);

void computeJointProbabilities(
    const std::map<int, std::list<std::pair<int, float>>> &Eout,
    std::map<int, std::list<std::pair<int, float>>> &EoutMerged);

void findTJunctions(const Image8 &segImg,
                    const std::vector<int> &bndToRegionId,
                    const std::vector<std::vector<Eigen::Vector2f>> &bnds,
                    const float maxDist,
                    std::map<int, std::list<std::pair<int, float>>> &E,
                    std::vector<Eigen::Vector2f> &outCentroids,
                    std::vector<float> &outInAngles,
                    std::vector<float> &outOutAngles,
                    std::vector<float> &outAngles);
void findTJunctions(const Image8 &segImg,
                    const std::vector<int> &bndToRegionId,
                    const std::vector<std::vector<Eigen::Vector2f>> &bnds,
                    const float maxDist,
                    std::map<int, std::list<std::pair<int, float>>> &E);

bool topologicalSort(
    const std::vector<int> &N,
    const std::map<int, std::list<std::pair<int, float>>> &Eout,
    std::vector<int> &NSorted);

#endif  // DEPTHINFALGS_H
