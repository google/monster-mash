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

#ifndef COMMONSTRUCTS_H
#define COMMONSTRUCTS_H

#include <miscutils/opengltools.h>

#include <Eigen/Dense>
#include <chrono>
#include <map>
#include <set>
#include <unordered_map>

#include "cpanim.h"
#include "def3dsdl.h"
#include "defengarapl.h"

typedef enum {
  DRAW_OUTLINE,
  DRAW_REGION_OUTLINE,
  DRAW_REGION,
  OUTLINE_RECOLOR,
  REGION_REDRAW_MODE,
  REGION_SWAP_MODE,
  REGION_MOVE_MODE,
  DEFORM_MODE,
  ANIMATE_MODE
} SubManipulationMode;
struct ManipulationMode {
  SubManipulationMode mode = DRAW_OUTLINE;
  ManipulationMode(){};
  ManipulationMode(const SubManipulationMode &mode) : mode(mode){};
  void setMode(const SubManipulationMode &mode) { this->mode = mode; }
  bool isImageModeActive() const { return mode < DEFORM_MODE; }
  bool isGeometryModeActive() const { return mode >= DEFORM_MODE; }
};

enum { ERASE_PIXEL_COLOR = 128, NEUMANN_OUTLINE_PIXEL_COLOR = 64 };

typedef enum {
  ANIM_MODE_TIME_SCALED,
  ANIM_MODE_OVERWRITE,
  ANIM_MODE_AVERAGE
} AnimMode;

struct GLData {
  GLMeshData meshData;
  GLuint shaderMatcap, shaderTexture;
  std::vector<GLuint> textureNames;
  GLuint templateImgTexName, textureImgTexName[4];
  GLuint backgroundImgTexName;
  Eigen::MatrixXd P, M;
};

struct ShadingOptions {
  bool showTexture = false;
  bool showTextureUseMatcapShading = true;
  int matcapImg = 0;
  bool showTemplateImg = false;
  bool showBackgroundImg = false;
  bool useNormalSmoothing = true;
  int normalSmoothingIters = 20;
  double normalSmoothingStep = 0.01;
};

struct CPData {
  // control point animations
  std::unordered_map<int, CPAnim> cpsAnim;
  CPAnim cpAnimSync;  // cpAnim used for synchronization only
  int cpsAnimSyncId = -1;
  bool recordCP = false;  // true when CP position is being recorded
  bool recordCPActive =
      false;  // true even when CP position has been recorded but LMB has not
              // been released yet (this prevents unintentional translation of
              // just recorded animation)
  bool recordCPWaitForClick = false;
  std::chrono::time_point<std::chrono::high_resolution_clock> timestampStart;
  std::string savedCPs;

  // control points
  int selectedPoint = -1;
  std::set<int> selectedPoints;

  bool playAnimation = true;
  bool playAnimWhenSelected = true;
  AnimMode animMode = ANIM_MODE_OVERWRITE;
  bool showControlPoints = true;
};

struct DefData {
  Def3D def;
  Def3D defDeformMode;
  DefEngARAPL defEng;
  Mesh3D mesh;
  std::vector<std::vector<int>> verticesOfParts;
  Eigen::MatrixXd VCurr, VRest, VRestOrig;
  Eigen::MatrixXd normals;  // per vertex normals
  Eigen::MatrixXi Faces;
  int defEngMaxIter = 4;
  double rigidity = 0.999;
};

struct ImgData {
  std::deque<Imguc> outlineImgs, regionImgs;
  std::deque<int> layers;  // ordered IDs of images
  Imguc mergedOutlinesImg, mergedRegionsImg, mergedRegionsOneColorImg,
      currOutlineImg, minRegionsImg;
  std::vector<int> layerGrayIds;
  int selectedLayer = -1;
  std::set<int> selectedLayers;
  bool showNeumannOutlines = false;
  bool showSelectedRegionOnly = false;
};

struct RecData {
  std::string triangleOpts = "pqa25QYY";
  int subsFactor = 2;
  bool armpitsStitching = false;
  bool armpitsStitchingInJointOptimization = false;
  int smoothFactor = 10;
  double defaultInflationAmount = 2;
  std::unordered_map<int, double> regionInflationAmount;
  std::set<int> mergeBothSides;
  int shiftModelX = 0;
  int shiftModelY = 0;
  int tempSmoothingSteps = 1;
  int searchThreshold = 5;
  bool cpOptimizeForZ = true;
  bool cpOptimizeForXY = false;
  bool interiorDepthConditions = false;
};

struct PauseStatus {
  bool defPaused = false;
  bool animPaused = false;
};

struct CameraData {
  bool ortho = true;
  float rotVer = 0;  // in degrees
  float rotHor = 0;  // in degrees
  Eigen::Vector2d rotPrev = Eigen::Vector2d::Zero();
  Eigen::Vector2d translateView = Eigen::Vector2d::Zero();
  Eigen::Vector2d transPrev = Eigen::Vector2d::Zero();
};

#endif  // COMMONSTRUCTS_H
