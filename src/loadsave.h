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

#ifndef LOADSAVE_H
#define LOADSAVE_H

#include <string>

#include "commonStructs.h"

bool saveControlPointsToStream(std::ostream &stream, CPData &cpData,
                               DefData &defData, ImgData &imgData);
bool saveControlPointsToFile(const std::string &fn,
                             const ManipulationMode &manipulationMode,
                             const std::string &savedCPs, CPData &cpData,
                             DefData &defData, ImgData &imgData);
bool loadControlPointsFromFile(const std::string &fn, std::string &savedCPs);
bool loadControlPointsFromStream(std::istream &stream, CPData &cpData,
                                 DefData &defData, ImgData &imgData);

void loadImages(const std::string &dir, const int viewportW,
                const int viewportH, ImgData &imgData, RecData &recData);
void loadAllFromDir(const std::string &dir, const int viewportW,
                    const int viewportH, ImgData &imgData, RecData &recData,
                    std::string &savedCPs);
void loadAllFromZip(const std::string &zipFn, const int viewportW,
                    const int viewportH, CPData &cpData, ImgData &imgData,
                    RecData &recData, std::string &savedCPs, Imguc &templateImg,
                    Imguc &backgroundImg, ShadingOptions &shadingOpts,
                    ManipulationMode &manipulationMode,
                    bool &middleMouseSimulation);
void saveAllToDir(const std::string &dir,
                  const ManipulationMode &manipulationMode,
                  const std::string &savedCPs, CPData &cpData, DefData &defData,
                  ImgData &imgData, RecData &recData);
void saveAllToZip(const std::string &zipFn, CPData &cpData, DefData &defData,
                  ImgData &imgData, RecData &recData,
                  const std::string &savedCPs, const Imguc &templateImg,
                  const Imguc &backgroundImg, ShadingOptions &shadingOpts,
                  ManipulationMode &manipulationMode,
                  bool middleMouseSimulation);

#endif  // LOADSAVE_H
