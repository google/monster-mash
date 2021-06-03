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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "commonStructs.h"
#include "exportgltf.h"
#include "mywindow.h"

class MainWindow : public MyWindow {
 public:
  MainWindow(int w, int h, const std::string &windowTitle);
  virtual ~MainWindow();

  void deselectAll();
  void selectAll();
  bool copySelectedAnim();
  bool pasteSelectedAnim();
  void loadTemplateImageFromFile(bool scale = true);
  void setTemplateImageVisibility(bool visible);
  bool getTemplateImageVisibility();
  bool hasTemplateImage();
  void loadBackgroundImageFromFile(bool scale = true);
  void setBackgroundImageVisibility(bool visible);
  bool getBackgroundImageVisibility();
  void enableTextureShading(bool enabled = true);
  bool isTextureShadingEnabled();
  void setCPsVisibility(bool visible);
  bool getCPsVisibility();
  ManipulationMode openProject(const std::string &zipFn,
                               bool changeMode = true);
  void saveProject(const std::string &zipFn);
  AnimMode getAnimRecMode();
  bool isAnimationPlaying();
  ManipulationMode &getManipulationMode();
  void changeManipulationMode(const ManipulationMode &manipulationMode,
                              bool saveCPs = true);
  void cpRecordingRequestOrCancel();
  void toggleAnimationPlayback();
  void removeControlPointOrRegion();
  void reset();
  void recomputeCameraCenter();
  void enableArmpitsStitching(bool enabled = true);
  bool isArmpitsStitchingEnabled();
  void enableNormalSmoothing(bool enabled = true);
  bool isNormalSmoothingEnabled();
  void exportTextureTemplate(const std::string &fn);
  void enableMiddleMouseSimulation(bool enabled = true);
  bool isMiddleMouseSimulationEnabled();
  void resetView();
  void exportAsOBJ(const std::string &outDir,
                   const std::string &outFnWithoutExtension, bool saveTexture);

  // animation
  void offsetSelectedCpAnimsByFrames(double offset);
  void offsetSelectedCpAnimsByPercentage(double offset);
  void setAnimRecMode(AnimMode animMode);
  void exportAnimationStart(int preroll, bool solveForZ, bool perFrameNormals);
  void exportAnimationStop(bool exportModel = true);
  void exportAnimationFrame();
  bool exportAnimationRunning();
  void pauseAnimation();
  void resumeAnimation();
  int getNumberOfAnimationFrames();

 protected:
  bool paintEvent();
  virtual void fingerEvent(const MyFingerEvent &event);
  virtual void fingerMoveEvent(const MyFingerEvent &event);
  virtual void fingerPressEvent(const MyFingerEvent &event);
  virtual void fingerReleaseEvent(const MyFingerEvent &event);
  virtual void mouseEvent(const MyMouseEvent &event);
  virtual void mouseMoveEvent(const MyMouseEvent &event);
  virtual void mousePressEvent(const MyMouseEvent &event);
  virtual void mouseReleaseEvent(const MyMouseEvent &event);
  virtual void keyEvent(const MyKeyEvent &event);
  virtual void keyPressEvent(const MyKeyEvent &event);

 private:
  void initOpenGL();
  void destroyOpenGL();
  void drawModelOpenGL(Eigen::MatrixXd &V, Eigen::MatrixXd &Vr,
                       Eigen::MatrixXi &F, Eigen::MatrixXd &N);
  void pauseOrResumeZDeformation(bool pause);
  double handleDeformations();
  void startModeTransition(const ManipulationMode &prevMode,
                           const ManipulationMode &currMode);
  bool drawModeTransition(MyPainter &painter, MyPainter &painterOther);
  void computeNormals(bool smoothing);
  void drawGeometryMode(MyPainter &painterModel, MyPainter &painterOther);
  void rotateViewportIncrement(double rotHorInc, double rotVerInc);
  void drawModelSoftware(Eigen::MatrixXd &Vc, Eigen::MatrixXd &Vr,
                         Eigen::MatrixXi &F, Eigen::MatrixXd &N);
  void cpVisualize2D(MyPainter &screenPainter);
  void drawMessages(MyPainter &painter);
  int selectLayerUnderCoords(int x, int y, bool nearest);
  void handleMousePressEventImageMode(const MyMouseEvent &event);
  void handleMouseMoveEventImageMode(const MyMouseEvent &event);
  void handleMouseReleaseEventImageMode(const MyMouseEvent &event);
  void handleMousePressEventGeometryMode(const MyMouseEvent &event);
  void handleMouseMoveEventGeometryMode(const MyMouseEvent &event);
  void handleMouseReleaseEventGeometryMode(const MyMouseEvent &event);
  void drawLine(const MyMouseEvent &event, int x1, int y1, int x2, int y2,
                int thickness, const Cu &color);
  void drawImageMode(MyPainter &painter, MyPainter &painterOther,
                     double alpha = 1);
  void initImageLayers();
  void clearImgs();
  void recreateMergedImgs();
  void transformEnd(bool apply);
  void transformApply();
  void transformDiscard();
  void removeControlPoints();
  bool removeSelectedLayers();
  void removeLastRegion();
  void removeRegion(int regId);
  void deselectAllRegions();
  void deselectAllControlPoints();
  void selectAllControlPoints();
  void selectAllRegions();

  void pauseAll(PauseStatus &status);
  void resumeAll(PauseStatus &status);

  // animation
  void setRecordingCP(bool active);
  void cpAnimationPlaybackAndRecord();
  void toggleAnimationSelectedPlayback();
  void toggleAutoSmoothAnim();
  bool removeCPAnims();
  void visualizeCPAnimTrajectories2D(MyPainter &screenPainter);

  Img<float> depthBuffer;
  Imguc frameBuffer;
  const int circleRadius = 2;
  const int minPressReleaseDurationMs = 200;

  long long counter = 0;

  Eigen::Vector2d anchor, shift, translate, mouseCurrPos, mousePrevPos,
      mousePressStart, mouseReleaseEnd;
  ManipulationMode manipulationMode = ManipulationMode(DRAW_OUTLINE);
  Cu paintColor = Cu{0, 0, 0, 255};
  const float defaultThickness = 3;
  int drawingUnderLayer = std::numeric_limits<int>::max();
  bool rightMouseButtonSimulation2 = false;
  ;
  bool *rightMouseButtonSimulation = &rightMouseButtonSimulation2;
  bool hideRedAnnotations = false;
  bool showSegmentation = false;
  std::string progressMessage = "";
  bool showMessages = true;
  bool middleMouseSimulation = false;
  std::string datadirname = "../../data/";
  bool mousePressed = false;
  Cu bgColor = Cu{240, 240, 240, 255};

  // mode transition
  bool startTransition = false;
  bool transitionRunning = false;
  std::chrono::high_resolution_clock::time_point transitionStartTimepoint;
  int transitionDuration = 500;
  ManipulationMode transitionPrevMode, transitionCurrMode;
  CameraData transitionCamData;
  float rotHorFrom, rotHorTo, rotVerFrom, rotVerTo;

  Imguc screenImg;

  int viewportW;
  int viewportH;

  // image layers
  ImgData imgData;
  std::deque<Imguc> &outlineImgs = imgData.outlineImgs,
                    &regionImgs = imgData.regionImgs;
  std::deque<int> &layers = imgData.layers;
  Imguc &mergedOutlinesImg = imgData.mergedOutlinesImg;
  Imguc &mergedRegionsImg = imgData.mergedRegionsImg;
  Imguc &mergedRegionsOneColorImg = imgData.mergedRegionsOneColorImg;
  Imguc &currOutlineImg = imgData.currOutlineImg;
  Imguc &minRegionsImg = imgData.minRegionsImg;
  std::vector<int> &layerGrayIds = imgData.layerGrayIds;
  bool &showNeumannOutlines = imgData.showNeumannOutlines;
  bool &showSelectedRegionOnly = imgData.showSelectedRegionOnly;
  int &selectedLayer = imgData.selectedLayer;
  std::set<int> &selectedLayers = imgData.selectedLayers;
  Imguc templateImg, backgroundImg;

  // deformation
  DefData defData;
  Def3D &def = defData.def;
  Def3D &defDeformMode = defData.defDeformMode;
  std::vector<std::vector<int>> &verticesOfParts = defData.verticesOfParts;
  DefEngARAPL &defEng = defData.defEng;
  Mesh3D &mesh = defData.mesh;
  int &defEngMaxIter = defData.defEngMaxIter;
  double &rigidity = defData.rigidity;
  std::chrono::high_resolution_clock::time_point arapTimerLast;
  bool defPaused = false;

  // reconstruction
  RecData recData;
  std::string &triangleOpts = recData.triangleOpts;
  int &subsFactor = recData.subsFactor;
  bool &armpitsStitching = recData.armpitsStitching;
  bool &armpitsStitchingInJointOptimization =
      recData.armpitsStitchingInJointOptimization;
  int &smoothFactor = recData.smoothFactor;
  double &defaultInflationAmount = recData.defaultInflationAmount;
  std::unordered_map<int, double> &regionInflationAmount =
      recData.regionInflationAmount;
  std::set<int> &mergeBothSides = recData.mergeBothSides;
  int &shiftModelX = recData.shiftModelX;
  int &shiftModelY = recData.shiftModelY;
  int &tempSmoothingSteps = recData.tempSmoothingSteps;
  int &searchThreshold = recData.searchThreshold;
  bool &cpOptimizeForZ = recData.cpOptimizeForZ;
  bool &cpOptimizeForXY = recData.cpOptimizeForXY;
  bool &interiorDepthConditions = recData.interiorDepthConditions;

  // control points
  CPData cpData, cpDataBackup;
  std::string &savedCPs = cpData.savedCPs;
  std::set<int> &selectedPoints = cpData.selectedPoints;
  int &selectedPoint = cpData.selectedPoint;
  std::unordered_map<int, CPAnim> &cpsAnim = cpData.cpsAnim;
  bool temporaryPoint = false;
  bool rubberBandActive = false;
  std::map<int, int> touchPointId2CP;

  // control points transform
  bool transformRelative = false;
  typedef enum { TRANSLATE, SCALE, ROTATE } TransformRelativeType;
  TransformRelativeType transformRelativeType = TRANSLATE;
  struct TransformRelativeRestriction {
    bool x = false;
    bool y = false;
    bool z = false;
  };
  TransformRelativeRestriction transformRelativeRestriction;
  Eigen::Vector2d transformStartPos;
  typedef enum {
    TRANS_PIVOT_CENTROID_COMMON,
    TRANS_PIVOT_CENTROID_INDIVIDUAL,
    TRANS_PIVOT_CENTROID_LAST
  } TransformPivot;
  TransformPivot transformPivot = TRANS_PIVOT_CENTROID_INDIVIDUAL;
  Eigen::Vector3d transformRelativeCentroid;

  // 3D view
  Eigen::MatrixXd proj3DView = Eigen::Matrix4d::Identity();
  Eigen::MatrixXd proj3DViewInv = Eigen::Matrix4d::Identity();
  Eigen::MatrixXd flyCameraM = Eigen::Matrix4d::Identity();
  float rotFactor = 0.5;
  Eigen::Vector3d cameraCenter = Eigen::Vector3d::Zero();
  Eigen::Vector3d cameraCenterViewSpace = Eigen::Vector3d::Zero();
  int autoRotateDir = 0;
  const float translateViewFactor = 1;
  Eigen::Vector3d translateCam = Eigen::Vector3d::Zero();
  Eigen::Vector3d rotateCam = Eigen::Vector3d::Zero();
  std::vector<std::pair<Eigen::Vector3d, Eigen::Vector3d>> cameraTransforms;
  CameraData camDataDeformMode = {
      .rotVer = 15, .rotHor = 30, .rotPrev = Eigen::Vector2d(30, 15)};
  CameraData camData, camDataAnimateMode;
  bool &ortho = camData.ortho;
  float &rotVer = camData.rotVer;
  float &rotHor = camData.rotHor;
  Eigen::Vector2d &translateView = camData.translateView;
  Eigen::Vector2d &rotPrev = camData.rotPrev;
  Eigen::Vector2d &transPrev = camData.transPrev;
  bool forceRecomputeCameraCenter = true;
  bool rotationModeActive = false;

  // opengl
  GLData glData;

  // visualization
  bool use3DCPVisualization = false;
  bool use3DCPTrajectoryVisualization = false;
  bool imageSpaceView = true;
  bool imageSpaceViewRotTrans = true;
  bool showModel = false;
  float scale = 1;

  // shading
  ShadingOptions shadingOpts;

  // animation
  bool manualTimepoint = false;
  bool enableMetronome = false;
  bool displayKeyframes = false;
  bool displaySyncCPAnim = false;
  bool autoSmoothAnim = true;
  int autoSmoothAnimFrom = -5, autoSmoothAnimTo = 5, autoSmoothAnimIts = 5;
  CPAnim copiedAnim;
  CPAnim &cpAnimSync = cpData.cpAnimSync;
  int exportAnimationPreroll = 0;
  bool exportPerFrameNormals = false;
  bool exportAnimationWaitForBeginning = true;
  int exportedFrames = 0;
  exportgltf::ExportGltf *gltfExporter = nullptr;
  exportgltf::MatrixXfR exportBaseV, exportBaseN;
  PauseStatus animStatus;
};

#endif  // MAINWINDOW_H
