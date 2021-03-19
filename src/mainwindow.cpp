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

#include "mainwindow.h"

#include <igl/per_vertex_normals.h>
#include <igl/writeOBJ.h>
#include <image/imageUtils.h>
#include <miscutils/camera.h>
#include <shaderMatcap/shaderMatcap.h>

#include "loadsave.h"
#include "macros.h"
#include "reconstruction.h"
#include "shaderTextureVertexCoords.h"

using namespace std;
using namespace igl;
using namespace Eigen;

MainWindow::MainWindow(int w, int h, const std::string &windowTitle)
    : MyWindow(w, h, windowTitle) {
  viewportW = w;
  viewportH = h;
  //  setMouseEventsSimulationByTouch(false);

  initOpenGL();
  initImageLayers();
  recreateMergedImgs();

  // main
  depthBuffer = Img<float>(windowWidth, windowHeight, 1);
  frameBuffer = Imguc(windowWidth, windowHeight, 4);

  // init screen image
  screenImg = Imguc(windowWidth, windowHeight, 4, 3);
}

MainWindow::~MainWindow() { destroyOpenGL(); }

void MainWindow::initOpenGL() {
  // opengl
  GLMeshInitBuffers(glData.meshData);
  glData.shaderMatcap = loadShaders(SHADERMATCAP_VERT, SHADERMATCAP_FRAG);
  glData.shaderTexture =
      loadShaders(SHADERTEXVERTCOORDS_VERT, SHADERTEXVERTCOORDS_FRAG);
  deque<string> textureFns{
      datadirname + "/shaders/matcapOrange.jpg",
  };
  loadTexturesToGPU(textureFns, glData.textureNames);
  if (shadingOpts.matcapImg != -1)
    glBindTexture(GL_TEXTURE_2D, glData.textureNames[shadingOpts.matcapImg]);
}

void MainWindow::destroyOpenGL() { GLMeshDestroyBuffers(glData.meshData); }

bool MainWindow::paintEvent() {
  if (!repaint) {
    SDL_Delay(1);
    return false;
  }
  repaint = false;
  MEASURE_TIME_START(tStartAll);

  MyPainter painter(this);

  painter.setColor(bgColor);
  painter.clearToColor();
  rasterizeGPUClear();
  screenImg.fill(0);
  MyPainter painterOther(screenImg);

  bool transitionRunning = drawModeTransition(painter, painterOther);
  if (transitionRunning) repaint = true;

  if ((manipulationMode.isGeometryModeActive() || showModel) &&
      !transitionRunning) {
    repaint = true;

    // draw background
    if (!backgroundImg.isNull() && shadingOpts.showBackgroundImg)
      painter.drawTexture(glData.backgroundImgTexName);

    // animation
    if (manipulationMode.mode == ANIMATE_MODE) {
      cpAnimationPlaybackAndRecord();
    }

    // draw 3D model
    computeNormals(shadingOpts.useNormalSmoothing);
    drawGeometryMode(painter, painterOther);

    // deformation
    if (!defPaused) {
      double defDiff = handleDeformations();
      //    DEBUG_CMD_MM(cout << defDiff << endl;);
      if (defDiff < 0.01 &&
          !(manipulationMode.mode == ANIMATE_MODE && isAnimationPlaying() &&
            !cpData.cpsAnim
                 .empty())  // always repaint if there's animation playing
      ) {
        repaint = false;
      }
    } else
      repaint = false;

    // export animation frame
    //    cout << exportAnimationRunning() << endl;
    if (exportAnimationRunning()) exportAnimationFrame();

    if (showMessages) drawMessages(painter);
  }
  if (manipulationMode.isImageModeActive() && !transitionRunning) {
    drawImageMode(painter, painterOther);
    painterOther.paint();
  }

  if (!transitionRunning) painter.drawImage(0, 0, screenImg);
  painter.paint();

  double tAllElapsed = 0;
  MEASURE_TIME_END(tStartAll, tAllElapsed);
#ifndef __EMSCRIPTEN__
//  cout << tAllElapsed << "ms, " << 1000.0/tAllElapsed << " FPS" << endl;
#endif

  const int delay = 8 - tAllElapsed;
  if (delay > 0) SDL_Delay(delay);

  return true;
}

double MainWindow::handleDeformations() {
  double tElapsedMsARAP = 0;
  auto *defCurr = &def;
  if (manipulationMode.mode == DEFORM_MODE) defCurr = &defDeformMode;
  if (mesh.VCurr.rows() == 0) {
    // mesh empty, skipping
    return 0;
  }
  double defDiff;
  MEASURE_TIME(defDiff = defEng.deform(*defCurr, mesh), tElapsedMsARAP);
  defData.VCurr = mesh.VCurr;
  defData.Faces = mesh.F;
  defData.VRestOrig = mesh.VRest;
  return defDiff;
}

void MainWindow::startModeTransition(const ManipulationMode &prevMode,
                                     const ManipulationMode &currMode) {
  transitionPrevMode = prevMode;
  transitionCurrMode = currMode;
  startTransition = true;
  repaint = true;
}

bool MainWindow::drawModeTransition(MyPainter &painter,
                                    MyPainter &painterOther) {
  if (startTransition) {
    recomputeCameraCenter();

    transitionStartTimepoint = chrono::high_resolution_clock::now();
    startTransition = false;
    transitionRunning = true;
    transitionCamData = camData;

    rotVerTo = 0, rotVerFrom = 0, rotHorTo = 0, rotHorFrom = 0;
    rotHorFrom = transitionCamData.rotHor;
    rotVerFrom = transitionCamData.rotVer;
    if (transitionCurrMode.mode == DEFORM_MODE) {
      rotHorTo = camDataDeformMode.rotHor;
      rotVerTo = camDataDeformMode.rotVer;
    } else if (transitionCurrMode.mode == ANIMATE_MODE) {
      rotHorTo = camDataAnimateMode.rotHor;
      rotVerTo = camDataAnimateMode.rotVer;
    }

    // choose the shortest path around the circle
    auto angPos360 = [](float ang) -> float {
      ang = fmod(ang, 360);
      if (ang < 0) ang = 360 + ang;
      return ang;
    };
    auto shortest = [&](float fromIn, float toIn, float &fromOut,
                        float &toOut) {
      fromIn = angPos360(fromIn);
      toIn = angPos360(toIn);
      float d1 = toIn - fromIn;      // CCW
      float d2 = -toIn - (-fromIn);  // CW
      d1 = angPos360(d1);
      d2 = angPos360(d2);
      DEBUG_CMD_MM(cout << "fromIn: " << fromIn << ", toIn: " << toIn
                        << ", d1: " << d1 << ", d2: " << d2 << endl;);
      fromOut = fromIn;
      if (d1 < d2) {
        toOut = fromIn + d1;
      } else {
        toOut = fromIn - d2;
      }
    };

    shortest(rotHorFrom, rotHorTo, rotHorFrom, rotHorTo);
    shortest(rotVerFrom, rotVerTo, rotVerFrom, rotVerTo);
  }

  if (!transitionRunning) return false;

  int duration =
      chrono::duration_cast<chrono::milliseconds>(
          chrono::high_resolution_clock::now() - transitionStartTimepoint)
          .count();
  if (duration >= transitionDuration) {
    transitionRunning = false;
    return false;
  }

  if (transitionPrevMode.isImageModeActive() &&
      transitionCurrMode.isImageModeActive()) {
    transitionRunning = false;
    return false;
  }

  auto lerp = [](const auto &to, const auto &from, double t) -> auto {
    return t * to + (1 - t) * from;
  };
  double t = static_cast<double>(duration) / transitionDuration;
  rotHor = lerp(rotHorTo, rotHorFrom, t);
  rotVer = lerp(rotVerTo, rotVerFrom, t);
  rotPrev = Vector2d(rotHor, rotVer);

  // draw background
  if (!backgroundImg.isNull() && shadingOpts.showBackgroundImg)
    painter.drawTexture(glData.backgroundImgTexName);
  // draw 3D model
  computeNormals(shadingOpts.useNormalSmoothing);
  drawGeometryMode(painter, painterOther);
  // draw sketch
  double t2 = 0;
  if (transitionCurrMode.isImageModeActive())
    t2 = t;
  else if (transitionPrevMode.isImageModeActive())
    t2 = 1 - t;
  if (t2 > 0) {
    if (transitionPrevMode.mode == ANIMATE_MODE) {
      // we assume that screenImg contains geometry mode rasterized information
      // only
      painter.drawImage(0, 0, screenImg, 1 - t2);
    }
    Imguc bg(windowWidth, windowHeight, 4, 3);
    bg.fill(bgColor);
    painter.drawImage(0, 0, bg, t2);
    drawImageMode(painter, painterOther, t2);
  }

  return true;
}

void MainWindow::computeNormals(bool smoothing) {
  // compute normals
  auto &V = defData.VCurr;
  auto &F = defData.Faces;
  auto &N = defData.normals;

  if (V.rows() == 0) {
    // mesh empty, skipping
    return;
  }

  per_vertex_normals(V, F, PER_VERTEX_NORMALS_WEIGHTING_TYPE_DEFAULT, N);
  // normals may have some NaN's, treat them before smoothing
  fora(i, 0, N.rows()) {
    bool nan = false;
    fora(j, 0, 3) {
      if (isnan(N(i, j))) {
        nan = true;
        break;
      }
    }
    if (nan) {
      N.row(i) = Vector3d(0, 0, 1);
    }
  }

  if (smoothing) {
    auto &L = defEng.L;
    if (L.rows() == N.rows()) {
      fora(i, 0, shadingOpts.normalSmoothingIters) {
        N += shadingOpts.normalSmoothingStep * L * N;
        N.rowwise().normalize();
      }
    }
  }
}

void MainWindow::drawGeometryMode(MyPainter &painterModel,
                                  MyPainter &painterOther) {
  auto *defData = &this->defData;
  auto &V = defData->VCurr;
  auto &Vr = defData->VRestOrig;
  auto &F = defData->Faces;
  auto &N = defData->normals;
  auto &showControlPoints = cpData.showControlPoints;

  if (false && autoRotateDir != -1 && manipulationMode.mode == DEFORM_MODE) {
    // auto rotate
    int redrawFPS = 60;
    if (redrawFPS > 0) {
      const double rot = 50.0 / redrawFPS;
      double dHor = 0, dVer = 0;
      if (autoRotateDir == 0)
        dHor = rot;
      else if (autoRotateDir == 1)
        dHor = -rot;
      else if (autoRotateDir == 2)
        dVer = rot;
      else if (autoRotateDir == 3)
        dVer = -rot;
      if (autoRotateDir != -1) rotateViewportIncrement(dHor, dVer);
    }
  }

  if (V.rows() == 0) {
    // mesh empty, skipping
    return;
  }

  // draw the 3D view
  const Vector3d &centroid = cameraCenter;
  if (forceRecomputeCameraCenter) {
    recomputeCameraCenter();
    forceRecomputeCameraCenter = false;
  }

  float near = -5000;
  float far = 10000;
  float scale2 = scale;
  if (!ortho) {
    float maxDepth =
        std::max(Vr.col(0).maxCoeff() - Vr.col(0).minCoeff(),
                 std::max(Vr.col(1).maxCoeff() - Vr.col(1).minCoeff(),
                          Vr.col(2).maxCoeff() - Vr.col(2).minCoeff()));
    near = 500;
    far = near + 1.5 * maxDepth;
    scale2 *= 1.5;
  }

  float rotHor2 = rotHor, rotVer2 = rotVer;
  Vector2d translateView2 = translateView;
  if (imageSpaceView) {
    ortho = true;
    if (!imageSpaceViewRotTrans) {
      rotHor2 = 0;
      rotVer2 = 0;
    }
    translateView2.fill(0);
    near = -5000;
    far = 10000;
  }

  buildCameraMatrices(ortho, viewportW / scale2, viewportH / scale2, near, far,
                      rotVer2, rotHor2, translateView2, centroid, glData.P,
                      glData.M);
  if (imageSpaceView) {
    // translate in image space
    const Vector3d shift =
        (glData.P * (cameraCenterViewSpace +
                     Vector3d(translateView.x(), translateView.y(), 0))
                        .homogeneous())
            .hnormalized();
    glData.P = Affine3d(Translation3d(Vector3d(shift(0), shift(1), 0) +
                                      Vector3d(-1, -1, 0))) *
               Ref<Matrix4d>(glData.P);
  }
  flyCameraM =
      Affine3d(                        // from bottom to top
          Translation3d(translateCam)  // translate
          *
          AngleAxisd(rotateCam(2) / 180.0 * M_PI, Vector3d::UnitZ())  // rotate
          *
          AngleAxisd(rotateCam(1) / 180.0 * M_PI, Vector3d::UnitY())  // rotate
          *
          AngleAxisd(rotateCam(0) / 180.0 * M_PI, Vector3d::UnitX())  // rotate
          )
          .matrix();
  glData.M = flyCameraM * glData.M;
  proj3DView = Affine3d(Scaling<double>(0.5 * viewportW, 0.5 * viewportH, 1) *
                        Translation3d(1, 1, 0)) *
               Ref<Matrix4d>(glData.P) * Ref<Matrix4d>(glData.M);
  proj3DViewInv = proj3DView.inverse();

  glData.P =
      Affine3d(Scaling(1., -1., 1.)).matrix() *
      glData
          .P;  // convert from cartesian (OpenGL) to image coordinates (flip y)

  glViewport(0, 0, viewportW, viewportH);
  drawModelOpenGL(V, Vr, F, N);

  if (showControlPoints) {
    if (manipulationMode.mode == ANIMATE_MODE) {
      if (!use3DCPTrajectoryVisualization)
        visualizeCPAnimTrajectories2D(painterOther);
    }
    if (!use3DCPVisualization) {
      cpVisualize2D(painterOther);
    }
  }

  // draw rubber band
  if (rubberBandActive) {
    double x1 = mousePressStart(0);
    double y1 = mousePressStart(1);
    double x2 = mouseCurrPos(0);
    double y2 = mouseCurrPos(1);
    if (x1 > x2) swap(x1, x2);
    if (y1 > y2) swap(y1, y2);
    painterOther.setColor(0, 0, 0, 255);
    painterOther.drawRect(x1, y1, x2, y2);
  }
}

void MainWindow::rotateViewportIncrement(double rotHorInc, double rotVerInc) {
  rotPrev += Vector2d(rotHorInc, rotVerInc);
  rotHor = rotPrev(0);
  rotVer = rotPrev(1);
}

void MainWindow::drawModelOpenGL(Eigen::MatrixXd &V, Eigen::MatrixXd &Vr,
                                 Eigen::MatrixXi &F, Eigen::MatrixXd &N) {
  auto *templateImg = &this->templateImg;

  MatrixXd colors;
  MatrixXd textureCoords;
  MatrixXi PARTID;
  GLuint activeShader;
  glActiveTexture(GL_TEXTURE0);
  if (shadingOpts.showTexture && !templateImg->isNull()) {
    activeShader = glData.shaderTexture;
    glUseProgram(activeShader);
    glUniform1i(glGetUniformLocation(activeShader, "tex"), 0);
    glUniform1f(glGetUniformLocation(activeShader, "useShading"),
                shadingOpts.showTextureUseMatcapShading ? 1 : 0);
    // texture coords
    const Imguc &I = *templateImg;
    textureCoords = (Vr.array().rowwise() / Array3d(I.w, I.h, 1).transpose());
    textureCoords.col(2).fill(-1);
    glBindTexture(GL_TEXTURE_2D, glData.templateImgTexName);
  } else if (shadingOpts.matcapImg != -1) {
    activeShader = glData.shaderMatcap;
    glUseProgram(activeShader);
    glBindTexture(GL_TEXTURE_2D, glData.textureNames[shadingOpts.matcapImg]);
  }

  MatrixXd C;
  uploadCameraMatrices(activeShader, glData.P, glData.M,
                       glData.M.inverse().transpose());
  GLMeshFillBuffers(activeShader, glData.meshData, V, F, N, textureCoords, C,
                    PARTID);
  GLMeshDraw(glData.meshData, GL_TRIANGLES);
}

void MainWindow::cpVisualize2D(MyPainter &screenPainter) {
  auto *cpData = &this->cpData;
  auto *defData = &this->defData;
  auto &selectedPoints = cpData->selectedPoints;
  auto &cpAnimSync = cpData->cpAnimSync;
  auto cpsAnimSyncId = cpData->cpsAnimSyncId;

  auto *defCurr = &def;
  if (manipulationMode.mode == DEFORM_MODE) {
    return;
  }

  Cu colorBlack{0, 0, 0, 255};
  Cu colorGreen{0, 255, 0, 255};
  Cu colorRed{255, 0, 0, 255};
  if (rotationModeActive) {
    colorBlack = Cu{128, 128, 128, 255};
    colorGreen = Cu{192, 192, 192, 255};
    colorRed = Cu{192, 192, 192, 255};
  }

  // show handles
  drawControlPoints(*defCurr, screenPainter, 7, proj3DView, 1, colorBlack,
                    colorRed);  // 3D view

  if (!displaySyncCPAnim && cpsAnimSyncId != -1) {
    try {
      const Vector3d p = defCurr->getCP(cpsAnimSyncId).pos;
      drawControlPoint(p, screenPainter, 7, proj3DView, 1, colorBlack,
                       colorGreen);
    } catch (out_of_range &e) {
      cerr << e.what() << endl;
    }
  }
  for (int cpId : selectedPoints) {
    try {
      Cu colorFg = colorRed;
      if (!displaySyncCPAnim && cpId == cpsAnimSyncId) colorFg = colorGreen;
      const Eigen::VectorXd &p = defCurr->getCP(cpId).pos;
      drawControlPoint(p, screenPainter, 7, proj3DView, 4, colorBlack, colorFg);
    } catch (out_of_range &e) {
      cerr << e.what() << endl;
    }
  }
}

void MainWindow::visualizeCPAnimTrajectories2D(MyPainter &screenPainter) {
  auto *cpData = &this->cpData;
  auto &cpsAnim = cpData->cpsAnim;
  auto &selectedPoints = cpData->selectedPoints;
  auto &cpAnimSync = cpData->cpAnimSync;

  // visualize animations of control points
  auto drawTrajectoriesAndKeyframes = [&](CPAnim &a,
                                          float beginningThicknessMult) {
    a.drawTrajectory(screenPainter, proj3DView,
                     beginningThicknessMult);  // 3D view
    if (displayKeyframes) {
      screenPainter.setThickness(1);
      a.drawKeyframes(screenPainter, 1, proj3DView);
    }
  };
  for (auto &el : cpsAnim) {
    // draw control points trajectory
    const int cpId = el.first;
    CPAnim &cpAnim = el.second;
    Cu black{0, 0, 0, 255};
    if (rotationModeActive) black = Cu{128, 128, 128, 255};
    screenPainter.setColor(black);
    float beginningThicknessMult = 2;
    if (selectedPoints.find(cpId) != selectedPoints.end()) {
      screenPainter.setThickness(3);
    } else {
      screenPainter.setThickness(1);
      beginningThicknessMult = 4;
    }
    drawTrajectoriesAndKeyframes(cpAnim, beginningThicknessMult);
  }
  // draw control points trajectory of sync anim
  if (displaySyncCPAnim) {
    screenPainter.setColor(0, 255, 0, 255);
    screenPainter.setThickness(1);
    drawTrajectoriesAndKeyframes(cpAnimSync, 2);
  }
}

void MainWindow::drawMessages(MyPainter &painter) {}

void MainWindow::pauseOrResumeZDeformation(bool pause) {}

void MainWindow::fingerEvent(const MyFingerEvent &event) {}

void MainWindow::fingerMoveEvent(const MyFingerEvent &event) {
  if (manipulationMode.isGeometryModeActive()) {
    defEng.solveForZ = false;  // pause z-deformation
    autoRotateDir = -1;
  }
}

void MainWindow::fingerPressEvent(const MyFingerEvent &event) {
  fingerMoveEvent(event);
}

void MainWindow::fingerReleaseEvent(const MyFingerEvent &event) {
  if (touchPointId2CP.find(event.fingerId) != touchPointId2CP.end())
    touchPointId2CP.erase(event.fingerId);

  if (manipulationMode.isGeometryModeActive()) {
    defEng.solveForZ = true;  // resume z-deformation
    autoRotateDir = 0;
  }
}

void MainWindow::mouseEvent(const MyMouseEvent &event) {
  if (event.leftButton || event.middleButton || event.rightButton)
    repaint = true;
}

void MainWindow::mouseMoveEvent(const MyMouseEvent &event) {
  mousePrevPos = mouseCurrPos;
  mouseCurrPos = Vector2d(event.pos.x(), event.pos.y());

  if (manipulationMode.isGeometryModeActive()) {
    if (event.leftButton || event.middleButton || event.rightButton) {
      defEng.solveForZ = false;  // pause z-deformation
      autoRotateDir = -1;
    }
    handleMouseMoveEventGeometryMode(event);
  } else {
    handleMouseMoveEventImageMode(event);
  }
}

void MainWindow::mousePressEvent(const MyMouseEvent &event) {
  mousePressStart = mousePrevPos = mouseCurrPos =
      Vector2d(event.pos.x(), event.pos.y());
  mousePressed = true;

  if (manipulationMode.isGeometryModeActive()) {
    defEng.solveForZ = false;  // pause z-deformation
    autoRotateDir = -1;
    handleMousePressEventGeometryMode(event);
  } else {
    handleMousePressEventImageMode(event);
  }
}

void MainWindow::mouseReleaseEvent(const MyMouseEvent &event) {
  mousePrevPos = mouseCurrPos;
  mouseReleaseEnd = mouseCurrPos = Vector2d(event.pos.x(), event.pos.y());
  mousePressed = false;

  if (manipulationMode.isGeometryModeActive()) {
    defEng.solveForZ = true;  // resume z-deformation
    autoRotateDir = 0;
    handleMouseReleaseEventGeometryMode(event);
  } else {
    handleMouseReleaseEventImageMode(event);
  }
}

void MainWindow::keyEvent(const MyKeyEvent &keyEvent) { repaint = true; }

void MainWindow::keyPressEvent(const MyKeyEvent &keyEvent) {
#ifndef __EMSCRIPTEN__
  if (keyEvent.key == SDLK_0) {
    if (getAnimRecMode() == ANIM_MODE_TIME_SCALED) {
      setAnimRecMode(ANIM_MODE_OVERWRITE);
    } else {
      setAnimRecMode(ANIM_MODE_TIME_SCALED);
    }
  }
  if (keyEvent.key == SDLK_F1) {
    showSegmentation = !showSegmentation;
  }
  if (keyEvent.key == SDLK_1) {
    changeManipulationMode(DRAW_OUTLINE);
  }
  if (keyEvent.key == SDLK_2) {
    changeManipulationMode(DEFORM_MODE);
  }
  if (keyEvent.key == SDLK_3) {
    changeManipulationMode(ANIMATE_MODE);
  }
  if (keyEvent.key == SDLK_4) {
    changeManipulationMode(REGION_SWAP_MODE);
  }
  if (keyEvent.key == SDLK_5) {
    changeManipulationMode(REGION_MOVE_MODE);
  }
  if (keyEvent.key == SDLK_6) {
    changeManipulationMode(REGION_REDRAW_MODE);
  }
  if (keyEvent.key == SDLK_e) {
    cpRecordingRequestOrCancel();
  }
  if (keyEvent.key == SDLK_SPACE) {
    toggleAnimationPlayback();
  }
  if (keyEvent.key == SDLK_n) {
    reset();
  }
  if (keyEvent.key == SDLK_s) {
    saveProject("/tmp/mm_project.zip");
  }
  if (keyEvent.key == SDLK_l) {
    changeManipulationMode(DRAW_REGION);
    openProject("/tmp/mm_project.zip");
  }
  if (keyEvent.key == SDLK_h) {
    setCPsVisibility(!getCPsVisibility());
  }
  if (keyEvent.key == SDLK_j) {
    setTemplateImageVisibility(!getTemplateImageVisibility());
  }
  if (keyEvent.key == SDLK_t) {
    loadTemplateImageFromFile();
  }
  if (keyEvent.key == SDLK_b) {
    loadBackgroundImageFromFile();
  }
  if (keyEvent.key == SDLK_o) {
    defEng.solveForZ = !defEng.solveForZ;
  }
  if (keyEvent.key == SDLK_p) {
    recData.armpitsStitching = !recData.armpitsStitching;
    DEBUG_CMD_MM(cout << "recData.armpitsStitching: "
                      << recData.armpitsStitching << endl;);
  }
  if (keyEvent.key == SDLK_w) {
    //    exportAsOBJ("/tmp", "mm_frame", true);

    //    writeOBJ("/tmp/mm_frame.obj", defData.VCurr, defData.Faces,
    //    defData.normals,
    //             defData.Faces, MatrixXd(), defData.Faces);

    if (!exportAnimationRunning())
      exportAnimationStart(0, false, false);
    else
      exportAnimationStop();
  }
  if (keyEvent.ctrlModifier) {
    if (keyEvent.key == SDLK_c) {
      // CTRL+C
      copySelectedAnim();
    }
    if (keyEvent.key == SDLK_v) {
      // CTRL+V
      pasteSelectedAnim();
    }
    if (keyEvent.key == SDLK_a) {
      // CTRL+A
      selectAll();
    }
  }
  if (keyEvent.key == SDLK_ESCAPE) {
    deselectAll();
  }
  if (keyEvent.key == SDLK_PLUS || keyEvent.key == SDLK_EQUALS ||
      keyEvent.key == SDLK_KP_PLUS) {
    offsetSelectedCpAnimsByFrames(1);
  }
  if (keyEvent.key == SDLK_MINUS || keyEvent.key == SDLK_KP_MINUS) {
    offsetSelectedCpAnimsByFrames(-1);
  }
  if (keyEvent.key == SDLK_DELETE || keyEvent.key == SDLK_BACKSPACE) {
    removeControlPointOrRegion();
  }
#endif
}

int MainWindow::selectLayerUnderCoords(int x, int y, bool nearest) {
  // select the farthest/nearest region
  int currSelectedLayer = -1;
  const int layerGrayId =
      nearest ? mergedRegionsImg(x, y, 0) : minRegionsImg(x, y, 0);
  if (layerGrayId > 0) {
    forlist(i, layerGrayIds) if (layerGrayIds[i] == layerGrayId)
        currSelectedLayer = i;
    DEBUG_CMD_MM(cout << "selected the " << (nearest ? "nearest" : "farthest")
                      << " layer: selectedLayer=" << currSelectedLayer << endl;)
  }
  return currSelectedLayer;
}

void MainWindow::handleMousePressEventImageMode(const MyMouseEvent &event) {
  auto &rightMouseButtonSimulation = *this->rightMouseButtonSimulation;

  if (event.leftButton || event.rightButton || event.middleButton) {
    drawLine(event, mousePrevPos(0), mousePrevPos(1), mouseCurrPos(0),
             mouseCurrPos(1), 2 * circleRadius, paintColor);
  }
  if ((event.leftButton || event.rightButton) &&
      (manipulationMode.mode == REGION_SWAP_MODE ||
       manipulationMode.mode == REGION_MOVE_MODE)) {
    const bool leftMouseButtonActive =
        event.leftButton && !rightMouseButtonSimulation;
    selectedLayer = selectLayerUnderCoords(mouseCurrPos.x(), mouseCurrPos.y(),
                                           leftMouseButtonActive);
  }
}

void MainWindow::handleMouseMoveEventImageMode(const MyMouseEvent &event) {
  if (event.leftButton || event.rightButton || event.middleButton) {
    // disable region modification (except in region redraw mode)
    if (manipulationMode.mode != REGION_REDRAW_MODE &&
        manipulationMode.mode != REGION_SWAP_MODE &&
        manipulationMode.mode != REGION_MOVE_MODE && selectedLayer != -1) {
      selectedLayers.clear();
      selectedLayer = -1;
      recreateMergedImgs();
    }

    drawLine(event, mousePrevPos(0), mousePrevPos(1), mouseCurrPos(0),
             mouseCurrPos(1), 2 * circleRadius, paintColor);
  }
}

void MainWindow::handleMouseReleaseEventImageMode(const MyMouseEvent &event) {
  auto *imgData = &this->imgData;
  auto &regionImgs = imgData->regionImgs;
  auto &outlineImgs = imgData->outlineImgs;
  auto &layers = imgData->layers;
  auto &currOutlineImg = imgData->currOutlineImg;
  auto &selectedLayer = this->selectedLayer;
  auto &layerGrayIds = imgData->layerGrayIds;
  auto &rightMouseButtonSimulation = *this->rightMouseButtonSimulation;
  const int selectedRegion = selectedLayer != -1 ? layers[selectedLayer] : -1;

  const bool leftMouseButtonActive =
      event.leftButton && !rightMouseButtonSimulation;
  const bool rightMouseButtonActive =
      event.rightButton || (event.leftButton && rightMouseButtonSimulation);

  if (event.rightButton) {
    drawLine(event, mousePrevPos(0), mousePrevPos(1), mouseCurrPos(0),
             mouseCurrPos(1), 2 * circleRadius, paintColor);
  }

  bool finishedDrawing = true;
  if (manipulationMode.isGeometryModeActive()) finishedDrawing = false;

  // compute stroke length
  const int minLength = 50;
  int length = 0;
  Imguc &Io = currOutlineImg;
  fora(y, 0, Io.h) fora(x, 0, Io.w) {
    if (Io(x, y, 0) != 255) length++;
    if (length > minLength) break;
  }

  const bool shortClick =
      event.pressReleaseDurationMs < minPressReleaseDurationMs;
  const bool doubleClick = event.numClicks >= 2;
  const bool shortStroke = length <= minLength;

  // perform region move or swap
  if (manipulationMode.mode == REGION_SWAP_MODE ||
      manipulationMode.mode == REGION_MOVE_MODE) {
    if (selectedLayer != -1) {
      const int currLayer = selectLayerUnderCoords(
          mouseReleaseEnd(0), mouseReleaseEnd(1), leftMouseButtonActive);
      if (currLayer != -1 && currLayer != selectedLayer) {
        if (manipulationMode.mode == REGION_SWAP_MODE) {
          // swap regions
          swap(layers[currLayer], layers[selectedLayer]);
        } else {
          // move selectedLayer in front of currLayer - NOT IMPLEMENTED!
        }
      }
    }
    selectedLayer = -1;
    selectedLayers.clear();
    finishedDrawing = false;
  } else {
    // region selection or duplication
    const bool shiftInDrawMode =
        manipulationMode.mode == DRAW_OUTLINE && event.shiftModifier;
    if (shiftInDrawMode || (shortStroke && shortClick && !doubleClick)) {
      // select the farthest/nearest region
      if (event.leftButton || event.rightButton) {
        const int selectedLayerCurr = selectLayerUnderCoords(
            mouseReleaseEnd(0), mouseReleaseEnd(1), leftMouseButtonActive);
        auto it = selectedLayers.find(selectedLayerCurr);
        if (shiftInDrawMode && (it != selectedLayers.end())) {
          // multiple selection active and the selected region is already in
          // selection, remove it from the selection
          selectedLayers.erase(it);
          if (selectedLayers.empty()) {
            selectedLayer = -1;
          } else {
            selectedLayer = *selectedLayers.begin();
          }
        } else {
          selectedLayer = selectedLayerCurr;
          // release mouse button outside of any region (or multiple selection
          // not active), deselect all layers
          if (selectedLayerCurr == -1 || !shiftInDrawMode)
            selectedLayers.clear();

          if (selectedLayerCurr != -1) selectedLayers.insert(selectedLayer);
        }
        finishedDrawing = false;
      }
    } else if (event.ctrlModifier ||
               (shortStroke && shortClick && doubleClick)) {
      // duplicate region and add it under/above everything drawn so far
      if (event.leftButton || event.rightButton) {
        const bool under = leftMouseButtonActive;
        const int selectedLayerCurr = selectLayerUnderCoords(
            mouseReleaseEnd(0), mouseReleaseEnd(1), leftMouseButtonActive);
        if (selectedLayerCurr != -1) {
          const int regId = layers[selectedLayerCurr];
          const Imguc &Ir = regionImgs[regId];
          int regIdDup = -1;
          forlist(i,
                  regionImgs) if (i != regId && Ir.equalData(regionImgs[i])) {
            regIdDup = i;
            break;
          }
          bool exists = regIdDup != -1;

          if (exists) {
            // region already has a duplicate, remove the farthest/nearest one
            const int regIdErase = regIdDup;
            outlineImgs[regIdErase].setNull();
            regionImgs[regIdErase].setNull();
            for (auto it = layers.begin(); it != layers.end(); it++) {
              if (*it == regIdErase) {
                layers.erase(it);
                break;
              }
            }
            DEBUG_CMD_MM(cout << "duplicate already exists, removing the "
                              << (under ? "farthest" : "nearest") << " one"
                              << endl;)
          } else {
            // region does not have a duplicate yet, duplicate it and move it
            // under/above everything drawn so far
            outlineImgs.push_back(outlineImgs[regId]);
            regionImgs.push_back(regionImgs[regId]);
            const int lastRegionId = regionImgs.size() - 1;
            if (under) {
              layers.push_front(lastRegionId);
            } else {
              layers.push_back(lastRegionId);
            }
            DEBUG_CMD_MM(cout << "adding a duplicate of region " << regId << " "
                              << (under ? "under" : "above")
                              << " everything drawn so far" << endl;)
          }
        }

        selectedLayer = -1;
        selectedLayers.clear();
      }

      finishedDrawing = false;
    }
  }

  if (shortClick) finishedDrawing = false;

  if (finishedDrawing) {
    const bool regionModify = selectedLayer != -1;
    const bool eraseMode = regionModify &&
                           manipulationMode.mode == DRAW_REGION &&
                           leftMouseButtonActive;
    const bool weightsMode = regionModify &&
                             manipulationMode.mode == DRAW_REGION &&
                             rightMouseButtonActive;
    const bool regionModifyRedraw =
        regionModify && manipulationMode.mode == REGION_REDRAW_MODE;

    if (event.leftButton || event.rightButton) {
      // create a region from the outline image
      Imguc *Io = &currOutlineImg;
      Imguc *Ir = nullptr;

      const int w = Io->w, h = Io->h;

      // check if the current outline is long enough to prevent from crashing at
      // later steps
      if (shortStroke) {
        DEBUG_CMD_MM(cout << "length of outline < " << minLength << ", skipping"
                          << endl;)
      } else {
        bool createOutlines = manipulationMode.mode == DRAW_REGION_OUTLINE;
        bool createRegions =
            manipulationMode.mode == DRAW_OUTLINE || regionModify;
        bool closeDrawnShape = false;

        if (weightsMode) {
          createRegions = createOutlines = false;
        } else if (regionModifyRedraw) {
          DEBUG_CMD_MM(cout << "region modification (redraw)" << endl;)
          // replace the outline of the selected region with the newly drawn one
          Ir = &regionImgs[selectedRegion];
          Imguc &I = outlineImgs[selectedRegion];
          I = *Io;
          *Ir = *Io;
          Io = &I;
          closeDrawnShape = true;
        } else if (regionModify || eraseMode) {
          // modifying a selected region
          Ir = &regionImgs[selectedRegion];
          // compose drawn outline image with the selected one
          Imguc &I = outlineImgs[selectedRegion];

          fora(y, 0, I.h) fora(x, 0, I.w) {
            if ((*Io)(x, y, 0) == 0)
              I(x, y, 0) = 0;
            else if ((*Io)(x, y, 0) == ERASE_PIXEL_COLOR) {
              if (manipulationMode.mode == OUTLINE_RECOLOR &&
                  I(x, y, 0) != 255) {
                if (leftMouseButtonActive)
                  I(x, y, 0) = 0;
                else if (rightMouseButtonActive)
                  I(x, y, 0) = NEUMANN_OUTLINE_PIXEL_COLOR;
              } else if (eraseMode)
                I(x, y, 0) = 255;
            }

            if ((*Io)(x, y, 0) == NEUMANN_OUTLINE_PIXEL_COLOR) {
              (*Ir)(x, y, 0) = 0;
              I(x, y, 0) = NEUMANN_OUTLINE_PIXEL_COLOR;
            } else {
              (*Ir)(x, y, 0) = I(x, y, 0);
            }
          }
          Io = &I;
        } else if (createOutlines || createRegions) {
          // insert a new layer under the layer specified using
          // drawingUnderLayer
          regionImgs.push_back(*Io);
          outlineImgs.push_back(*Io);
          Ir = &regionImgs.back();
          Io = &outlineImgs.back();
          const int lastRegionId = regionImgs.size() - 1;
          if (drawingUnderLayer != numeric_limits<int>::max()) {
            int underId = 0;
            forlist(i, layerGrayIds) if (layerGrayIds[i] == drawingUnderLayer)
                underId = i;
            const auto &pos = layers.begin() + underId;
            layers.insert(pos, lastRegionId);
          } else {
            layers.push_back(lastRegionId);
          }

          if (createRegions) closeDrawnShape = true;
        }

        if (closeDrawnShape) {
          // close the drawn shape
          Imguc tmp;
          tmp.initImage(*Io);
          tmp.fill(255);
          MyPainter painter(tmp);
          painter.setColor(NEUMANN_OUTLINE_PIXEL_COLOR,
                           NEUMANN_OUTLINE_PIXEL_COLOR,
                           NEUMANN_OUTLINE_PIXEL_COLOR, 255);
          painter.drawLine(mousePressStart(0), mousePressStart(1),
                           mouseReleaseEnd(0), mouseReleaseEnd(1),
                           defaultThickness);
          fora(y, 0, h)
              fora(x, 0, w) if ((*Io)(x, y, 0) == 255) (*Io)(x, y, 0) =
                  tmp(x, y, 0);  // put neumann outline behind regular one
          *Ir = *Io;
        }

        if (createRegions) {
          // if drawing outlines, fill the drawn shape
          // convert the shape to a region by filling the outside and recoloring
          // the remaining shape to have the same color (black)
          floodFill(*Ir, 0, 0, 255, 128);
          fora(y, 0, h) fora(x, 0, w) (*Ir)(x, y, 0) =
              (*Ir)(x, y, 0) == 128 ? 0 : 255;
        } else if (createOutlines) {
          // if drawing regions, create outline image by tracing the drawn black
          // region
          Io->fill(255);
          Imguc M;
          M.initImage(*Io);
          M.clear();
          Ir->invert(255);  // convert black regions to white, white background
                            // to black
          // find the first background pixel and use flood fill to add
          // background to the mask image
          int startX = -1, startY = -1;
          fora(y, 0, M.h) fora(x, 0, M.w) if ((*Ir)(x, y, 0) == 0) {
            startX = x;
            startY = y;
            goto EXIT;
          }
        EXIT:;
          floodFill(*Ir, M, startX, startY, 0, 255);
          auto bndVis = [&](const int x, const int y) {
            const int N = 1;
            fora(i, -N, N + 1)
                fora(j, -N, N + 1) if (Io->checkBounds(x + i, y + j, 0)) (*Io)(
                    x + i, y + j, 0) = 0;
          };
          while (traceRegion(M, bndVis, startX, startY)) {
            // if the region is composed of several separated components, find
            // one, remove it, repeat
            const int &color = (*Ir)(startX, startY, 0);
            floodFill(*Ir, M, startX, startY, color, 255);
          }
        }
      }
    }
  }

  // merge all regions into a single image (for displaying it)
  recreateMergedImgs();

  // clear the outline image
  currOutlineImg.fill(255);
}

void MainWindow::handleMousePressEventGeometryMode(const MyMouseEvent &event) {
  auto *cpData = &this->cpData;
  auto *defData = &this->defData;
  auto &selectedPoint = cpData->selectedPoint;
  auto &selectedPoints = cpData->selectedPoints;
  auto &def = defData->def;
  auto &VCurr = defData->VCurr;
  auto &Faces = defData->Faces;
  auto &rightMouseButtonSimulation = *this->rightMouseButtonSimulation;

  const bool leftMouseButtonActive =
      event.leftButton && !rightMouseButtonSimulation;
  const bool rightMouseButtonActive =
      event.rightButton || (event.leftButton && rightMouseButtonSimulation);

  if (!(VCurr.size() > 0 && proj3DView.size() > 0)) return;

  const int radius = 20;

  if (event.middleButton ||
      (middleMouseSimulation && (event.leftButton || event.rightButton))) {
    rotationModeActive = true;
    forceRecomputeCameraCenter = true;
    rubberBandActive = false;
    return;
  }

  if (manipulationMode.mode == DEFORM_MODE) {
    if (event.leftButton || event.rightButton) {
      bool reverse = true;
      if (leftMouseButtonActive) reverse = false;
      bool added = defDeformMode.addControlPointOnFace(
          VCurr, Faces, mouseCurrPos(0), mouseCurrPos(1), radius, reverse,
          reverse, selectedPoint, proj3DView);
      if (selectedPoint != -1) {
        selectedPoints.insert(selectedPoint);
      }
    }
    return;
  }

  if (transformRelative) {
    if (event.leftButton)
      transformApply();
    else
      transformDiscard();
    return;
  }

  if (event.leftButton || event.rightButton) {
    bool reverse = true;
    if (leftMouseButtonActive) reverse = false;

    bool added = def.addControlPointOnFace(VCurr, Faces, mouseCurrPos(0),
                                           mouseCurrPos(1), radius, reverse,
                                           reverse, selectedPoint, proj3DView);
    temporaryPoint = false;
    if (selectedPoint != -1) {
      if (!event.shiftModifier) {
        if (selectedPoints.find(selectedPoint) == selectedPoints.end())
          selectedPoints.clear();
        if (added) {
          //            temporaryPoint = true;
          temporaryPoint = false;
        }
      } else {
        // DISABLED: When adding a new permanent control point (using
        // Ctrl+LMB/RMB), add it to the current control point selection, i.e.
        // disable this: if (added & selectedPoints.find(selectedPoint) ==
        // selectedPoints.end()) selectedPoints.clear();
        if (added & selectedPoints.find(selectedPoint) == selectedPoints.end())
          selectedPoints.clear();
      }

      if (!added && event.shiftModifier &&
          selectedPoints.find(selectedPoint) != selectedPoints.end()) {
        selectedPoints.erase(selectedPoint);
        if (selectedPoints.empty()) selectedPoint = -1;
      } else {
        selectedPoints.insert(selectedPoint);
      }
    }

    if (selectedPoint == -1) {
      if (!event.shiftModifier) selectedPoints.clear();
      rubberBandActive = true;
    }
  }
}

void MainWindow::handleMouseMoveEventGeometryMode(const MyMouseEvent &event) {
  auto *cpData = &this->cpData;
  auto *defData = &this->defData;
  auto &selectedPoint = cpData->selectedPoint;
  auto &selectedPoints = cpData->selectedPoints;
  auto &recordCP = cpData->recordCP;
  auto &recordCPActive = cpData->recordCPActive;
  auto &cpsAnim = cpData->cpsAnim;
  auto &recordCPWaitForClick = cpData->recordCPWaitForClick;

  auto *defCurr = &def;
  if (manipulationMode.mode == DEFORM_MODE) defCurr = &defDeformMode;

  if (event.middleButton ||
      (middleMouseSimulation && (event.leftButton || event.rightButton))) {
    rotationModeActive = true;
    if (!event.shiftModifier) {
      const Vector2d r = rotPrev + rotFactor * (mouseCurrPos - mousePressStart);
      rotHor = r(0);
      rotVer = r(1);
    } else {
      // Shift+MiddleMouseButton pressed
      translateView =
          transPrev + translateViewFactor * (mouseCurrPos - mousePressStart);
    }
    return;
  }

  // if recording has been requested after clicking on a control point, start
  // recording
  if ((event.leftButton || event.rightButton) && recordCPWaitForClick &&
      !recordCP && selectedPoint != -1) {
    setRecordingCP(true);
  }

  if (!recordCP && recordCPActive) return;

  if (transformRelative) {
#if 0
    handleRelativeTransformation();
#endif
    return;
  }

  if (selectedPoint == -1) return;

  // Translation of selected control points and their animations (by dragging
  // only - relative transformation is handled in handleRelativeTransformation).
  Vector2d startPos;
  if (event.leftButton || event.rightButton) {
    startPos = mousePressStart;
  } else {
    return;
  }

  // compute translation vector (based on a single control point)
  try {
    auto &cpFirst = defCurr->getCP(*selectedPoints.begin());
    const Vector3d cpProj =
        (proj3DView * cpFirst.prevPos.homogeneous()).hnormalized();
    const Vector3d mouseCurrProj(mouseCurrPos(0), mouseCurrPos(1), cpProj(2));
    const Vector3d startPosProj(startPos(0), startPos(1), cpProj(2));
    const Vector3d t =
        (proj3DViewInv * mouseCurrProj.homogeneous()).hnormalized() -
        (proj3DViewInv * startPosProj.homogeneous()).hnormalized();

    // apply the translation vector to all selected control points
    for (const int cpId : selectedPoints) {
      auto &cp = defCurr->getCP(cpId);
      const auto &it = cpsAnim.find(cpId);
      if (it != cpsAnim.end() && !recordCP) {
        auto &cpAnim = it->second;
        Affine3d T((Translation3d(t(0), t(1), t(2))));
        cpAnim.setTransform(T.matrix());
        cp.pos = cpAnim.peek();
      } else {
        cp.pos = cp.prevPos + t;
      }
    }
  } catch (out_of_range &e) {
    cerr << e.what() << endl;
  }
}

void MainWindow::handleMouseReleaseEventGeometryMode(
    const MyMouseEvent &event) {
  auto *cpData = &this->cpData;
  auto *defData = &this->defData;
  auto &selectedPoint = cpData->selectedPoint;
  auto &selectedPoints = cpData->selectedPoints;
  auto &recordCPActive = cpData->recordCPActive;
  auto &defEng = defData->defEng;
  auto &mesh = defData->mesh;
  auto &verticesOfParts = defData->verticesOfParts;
  auto &VCurr = defData->VCurr;
  auto &Faces = defData->Faces;
  auto &rightMouseButtonSimulation = *this->rightMouseButtonSimulation;
  auto &recordCPWaitForClick = cpData->recordCPWaitForClick;

  const bool leftMouseButtonActive =
      event.leftButton && !rightMouseButtonSimulation;
  const bool rightMouseButtonActive =
      event.rightButton || (event.leftButton && rightMouseButtonSimulation);

  if (event.middleButton ||
      (middleMouseSimulation && (event.leftButton || event.rightButton))) {
    if (!event.shiftModifier) {
      rotPrev += rotFactor * (mouseCurrPos - mousePressStart);
    } else {
      // Shift+MiddleMouseButton pressed
      transPrev += translateViewFactor * (mouseCurrPos - mousePressStart);
    }

    if (!middleMouseSimulation) {
      rotationModeActive = false;
    }
    //    middleMouseSimulation = false;
    return;
  }

  if (manipulationMode.mode == DEFORM_MODE) {
    defDeformMode.removeControlPoints();
    selectedPoints.clear();
    selectedPoint = -1;
    return;
  }

  if (recordCPActive) {
    recordCPActive = false;

    // always stop recording CP after releasing the button
    setRecordingCP(false);
    recordCPWaitForClick = false;  // also stop recording mode
#ifdef __EMSCRIPTEN__
    EM_ASM(js_recordingModeStopped(););
#endif
    return;
  }

  if (transformRelative) {
    if (event.leftButton)
      transformApply();
    else
      transformDiscard();
    return;
  }

  transformApply();

  if (rubberBandActive) {
    // select points inside the rubber band
    double x1 = mousePressStart(0);
    double y1 = mousePressStart(1);
    double x2 = mouseReleaseEnd(0);
    double y2 = mouseReleaseEnd(1);
    if (x1 > x2) swap(x1, x2);
    if (y1 > y2) swap(y1, y2);
    const vector<int> &sel =
        def.getControlPointsInsideRect(x1, y1, x2, y2, proj3DView);
    if (!event.shiftModifier) selectedPoints.clear();
    for (const int id : sel) selectedPoints.insert(id);
    if (!selectedPoints.empty())
      selectedPoint = *selectedPoints.begin();
    else
      selectedPoint = -1;
    rubberBandActive = false;

    return;
  }

  if (!event.shiftModifier && (event.leftButton || event.rightButton)) {
    if (temporaryPoint) {
      def.removeLastControlPoint();
      selectedPoints.clear();
      selectedPoint = -1;
    }
  }
}

void MainWindow::drawLine(const MyMouseEvent &event, int x1, int y1, int x2,
                          int y2, int thickness, const Cu &color) {
  auto imgData = &this->imgData;
  auto &minRegionsImg = imgData->minRegionsImg;
  auto &currOutlineImg = imgData->currOutlineImg;
  auto &selectedLayer = this->selectedLayer;
  auto &mergedRegionsImg = imgData->mergedRegionsImg;
  auto &mergedOutlinesImg = imgData->mergedOutlinesImg;
  auto &rightMouseButtonSimulation = *this->rightMouseButtonSimulation;

  if (event.shiftModifier) return;
  if (event.ctrlModifier) return;
  if (manipulationMode.isGeometryModeActive()) return;
  if (manipulationMode.mode == REGION_SWAP_MODE ||
      manipulationMode.mode == REGION_MOVE_MODE)
    return;
  if (manipulationMode.mode == REGION_REDRAW_MODE && selectedLayer == -1)
    return;

  const bool leftMouseButtonActive =
      event.leftButton && !rightMouseButtonSimulation;
  const bool rightMouseButtonActive =
      event.rightButton || (event.leftButton && rightMouseButtonSimulation);

  const bool regionModify = false;  // disable original region modification
  const bool eraseMode = regionModify && manipulationMode.mode == DRAW_REGION &&
                         leftMouseButtonActive;
  const bool neumannOutline = regionModify &&
                              manipulationMode.mode == DRAW_OUTLINE &&
                              rightMouseButtonActive;
  const bool outlineRecolorMode =
      regionModify && manipulationMode.mode == OUTLINE_RECOLOR;

  // override the thickness if outline drawing mode is selected
  if (manipulationMode.mode == DRAW_OUTLINE ||
      manipulationMode.mode == REGION_REDRAW_MODE)
    thickness = defaultThickness;

  Cu c = color;
  if (neumannOutline)
    c = Cu{NEUMANN_OUTLINE_PIXEL_COLOR, NEUMANN_OUTLINE_PIXEL_COLOR,
           NEUMANN_OUTLINE_PIXEL_COLOR, 255};
  else if (eraseMode || outlineRecolorMode)
    c = Cu{ERASE_PIXEL_COLOR, ERASE_PIXEL_COLOR, ERASE_PIXEL_COLOR, 255};

  MyPainter painter(currOutlineImg);
  painter.setColor(c(0), c(1), c(2), c(3));
  painter.drawLine(x1, y1, x2, y2, thickness);

  drawingUnderLayer = numeric_limits<int>::max();
  Imguc &I = currOutlineImg, &MI = mergedOutlinesImg;
  if (neumannOutline) {
    fora(y, 0, I.h)
        fora(x, 0, I.w) if (I(x, y, 0) == NEUMANN_OUTLINE_PIXEL_COLOR) {
      MI(x, y, 1) = 255;
      MI(x, y, 2) = MI(x, y, 0) = 0;
      MI.alpha(x, y) = 255;  // green
    }
  } else if (eraseMode) {
    fora(y, 0, I.h) fora(x, 0, I.w) if (I(x, y, 0) == ERASE_PIXEL_COLOR) {
      MI.alpha(x, y) = 0;
    }
  } else if (outlineRecolorMode) {
  } else if (leftMouseButtonActive) {
    // compose current outline image on top of merged outlines image
    fora(y, 0, I.h) fora(x, 0, I.w) if (I(x, y, 0) == 0) {
      MI(x, y, 2) = MI(x, y, 1) = MI(x, y, 0) = 0;
      MI.alpha(x, y) = 255;
    }
  } else if (rightMouseButtonActive) {
    // When drawing under already drawn regions, change pen color and also
    // identify the backmost region.
    Imguc &Mr = mergedRegionsImg, &Mm = minRegionsImg;
    fora(y, 0, I.h) fora(x, 0, I.w) {
      if (I(x, y, 0) == 0) {
        if (Mm(x, y, 0) > 0 && Mm(x, y, 0) < drawingUnderLayer)
          drawingUnderLayer = Mm(x, y, 0);
        MI(x, y, 0) = Mm(x, y, 0) > 0 ? 220 : 0;
        MI(x, y, 2) = MI(x, y, 1) = MI(x, y, 0);
        MI.alpha(x, y) = 255;
      }
    }
  }
}

void MainWindow::drawImageMode(MyPainter &painter, MyPainter &painterOther,
                               double alpha) {
  auto *imgData = &this->imgData;
  auto &mergedRegionsImg = imgData->mergedRegionsImg;
  auto &mergedOutlinesImg = imgData->mergedOutlinesImg;
  auto &layers = imgData->layers;
  auto &selectedLayer = this->selectedLayer;
  auto &showTemplateImg = shadingOpts.showTemplateImg;

  // rasterize mesh
  painter.drawImage(0, 0, mergedRegionsOneColorImg, alpha);

  // show template image (used as a guide for tracing)
  if (showTemplateImg && !templateImg.isNull()) {
    painter.drawImage(0, 0, templateImg, alpha * 0.5);
  }

  // display segmented (depth) image
  if (showSegmentation) {
    fora(y, 0, viewportH) fora(x, 0, viewportW) {
      unsigned char alpha = 255;
      if (mergedRegionsImg(x, y, 0) == 0) alpha = 0;
      mergedRegionsImg(x, y, 3) = alpha;
    }
    painter.drawImage(0, 0, mergedRegionsImg, alpha);
  }
  // display outlines
  painter.drawImage(0, 0, mergedOutlinesImg, alpha);

  if ((manipulationMode.mode == REGION_SWAP_MODE ||
       manipulationMode.mode == REGION_MOVE_MODE) &&
      selectedLayer != -1) {
    painterOther.setColor(0, 255, 0, 255);
    const bool headStart = manipulationMode.mode == REGION_SWAP_MODE;
    painterOther.drawArrow(mousePressStart.x(), mousePressStart.y(),
                           mouseCurrPos.x(), mouseCurrPos.y(), 10, 30, 3, true,
                           headStart);
  }
}

// START OF IMAGE LAYERS
void MainWindow::initImageLayers() {
  currOutlineImg = Imguc(viewportW, viewportH, 1);
  mergedRegionsImg = Imguc(viewportW, viewportH, 4, 3);
  mergedRegionsOneColorImg = Imguc(viewportW, viewportH, 4, 3);
  mergedOutlinesImg = Imguc(viewportW, viewportH, 4, 3);
  minRegionsImg.initImage(currOutlineImg);

  clearImgs();
}

void MainWindow::clearImgs() {
  currOutlineImg.fill(255);
  outlineImgs.clear();
  regionImgs.clear();
  layers.clear();
  mergedRegionsImg.fill(Cu{0, 0, 0, 255});
  mergedRegionsOneColorImg.fill(0);
  mergedOutlinesImg.fill(0);
  minRegionsImg.clear();
  repaint = true;
}

void MainWindow::recreateMergedImgs() {
  Imguc &Mr = mergedRegionsImg, &Mo = mergedOutlinesImg, &Mm = minRegionsImg;
  Mr.fill(Cu{0, 0, 0, 255});
  /*Mr.clear();*/ Mo.fill(0);
  Mm.clear();
  const int w = Mr.w, h = Mr.h;
  const int N = layers.size();
  layerGrayIds.clear();
  fora(i, 0, N) {
    const int regId = layers[i];
    const int n = (i + 1) / (float)N * 255;
    layerGrayIds.push_back(n);
    Imguc &IrCurr = regionImgs[regId];
    Imguc &IoCurr = outlineImgs[regId];
    const bool selected = selectedLayers.find(i) != selectedLayers.end();
    fora(y, 0, h) fora(x, 0, w) {
      if (IrCurr(x, y, 0) == 255) {  // we're inside a region
        fora(c, 0, 3) Mr(x, y, c) = n;

        if (Mo.alpha(x, y) != 0) fora(c, 0, 3) {
            // light outlines
            int v = Mo(x, y, c);
            if (v == 0) v += 220;
            Mo(x, y, c) = v;
          }
        if (Mm(x, y, 0) == 0 || Mm(x, y, 0) > n) Mm(x, y, 0) = n;
      }
      if (IoCurr(x, y, 0) != 255) {  // we're inside a stroke
        if (IoCurr(x, y, 0) == NEUMANN_OUTLINE_PIXEL_COLOR) {
          if ((showNeumannOutlines || selected)) {
            Mo(x, y, 1) = 255;
            Mo(x, y, 0) = Mo(x, y, 2) = 0;
            Mo.alpha(x, y) = 255;
          }
        } else {
          if (selected) {
            Mo(x, y, 2) = 255;
            Mo(x, y, 0) = Mo(x, y, 1) = 0;
            Mo.alpha(x, y) = 255;
          } else if (!showSelectedRegionOnly) {
            if (Mo.alpha(x, y) != 0) {
              if (!hideRedAnnotations)
                Mo(x, y, 0) = 255;
              else
                Mo(x, y, 0) = 0;
              Mo(x, y, 2) = Mo(x, y, 1) = 0;
            } else {
              Mo(x, y, 0) = Mo(x, y, 2) = Mo(x, y, 1) = 0;
            }
            Mo.alpha(x, y) = 255;
          }
        }
      }
      if (IrCurr(x, y, 0) == 255) {  // we're inside a region
      }
    }
  }

  // create a one color region layer that will be placed under outline image to
  // better convey the regions
  mergedRegionsOneColorImg.fill(0);
  fora(y, 0, h) fora(x, 0, w) {
    if (Mr(x, y, 0) > 0) {
      fora(c, 0, 4) mergedRegionsOneColorImg(x, y, c) = 255;
    }
  }

  repaint = true;
}

void MainWindow::reset() {
  // reset image data
  clearImgs();
  selectedLayer = -1;
  selectedLayers.clear();
  templateImg.setNull();
  backgroundImg.setNull();
  showModel = false;

  // reset reconstruction data
  recData = RecData();

  // reset shading options
  shadingOpts = ShadingOptions();

  // reset control points
  cpData = CPData();
  cpDataBackup = CPData();

  // reset deformation
  defData = DefData();

  // reset camera
  camData = CameraData();
  translateCam = Eigen::Vector3d::Zero();
  rotateCam = Eigen::Vector3d::Zero();
  cameraTransforms.clear();
  forceRecomputeCameraCenter = true;
  enableMiddleMouseSimulation(false);

  // change manipulation mode to default
  manipulationMode = DRAW_OUTLINE;

  repaint = true;
}

void MainWindow::changeManipulationMode(
    const ManipulationMode &manipulationMode, bool saveCPs) {
  ManipulationMode prevMode = this->manipulationMode;
  this->manipulationMode = manipulationMode;

  // do not proceed if switching to the same mode
  if (prevMode.mode == manipulationMode.mode) {
    return;
  }

  DEBUG_CMD_MM(cout << "changeManipulationMode: " << prevMode.mode << " -> "
                    << manipulationMode.mode << endl;);

  // going from DEFORM_MODE
  if (prevMode.mode == DEFORM_MODE && manipulationMode.mode != DEFORM_MODE) {
    DEBUG_CMD_MM(cout << "restoring data from backup" << endl;);
    // restore cpData
    bool showControlPointsPrev = cpData.showControlPoints;
    cpData = cpDataBackup;
    cpData.showControlPoints = showControlPointsPrev;
  }

  // going from image to geometry mode
  if (prevMode.isImageModeActive() && manipulationMode.isGeometryModeActive()) {
    progressMessage = "Reconstruction running";

    if (performReconstruction(recData, defData, cpData, imgData)) {
      progressMessage = "";

      // reset camera matrix (needed for proper transitions)
      proj3DView = proj3DViewInv = Matrix4d::Identity();

    } else {
      progressMessage = "Reconstruction failed";
      changeManipulationMode(DRAW_OUTLINE);

#ifdef __EMSCRIPTEN__
      EM_ASM(js_reconstructionFailed(););
#endif

      return;
    }

#ifdef __EMSCRIPTEN__
    EM_ASM(js_reconstructionFinished(););
#endif
  }

  // going from geometry to image mode
  if (prevMode.isGeometryModeActive() && manipulationMode.isImageModeActive()) {
    if (saveCPs) {
      // save control points and their animations
      ostringstream oss;
      saveControlPointsToStream(oss, cpData, defData, imgData);
      savedCPs = oss.str();
    }

    recreateMergedImgs();
  }

  // staying in geometry mode
  if (prevMode.isGeometryModeActive() &&
      manipulationMode.isGeometryModeActive()) {
  }

  // going to DEFORM_MODE
  if (prevMode.mode != DEFORM_MODE && manipulationMode.mode == DEFORM_MODE) {
    DEBUG_CMD_MM(cout << "making data backup" << endl;);
    // make backup of cpData and clear it
    cpDataBackup = cpData;
    bool showControlPointsPrev = cpData.showControlPoints;
    cpData = CPData();
    cpData.showControlPoints = showControlPointsPrev;

    defEng.solveForZ = true;  // resume z-deformation
  }

  // going to REGION_REDRAW_MODE
  if (manipulationMode.mode == REGION_REDRAW_MODE) {
    if (selectedLayer != -1 && selectedLayers.size() > 1) {
      // only single selected region is allowed in REGION_REDRAW_MODE
      selectedLayers.clear();
      selectedLayers.insert(selectedLayer);
      recreateMergedImgs();
    }
  }

  startModeTransition(prevMode, manipulationMode);
  repaint = true;
}

void MainWindow::transformEnd(bool apply) {
  auto *cpData = &this->cpData;
  auto *defData = &this->defData;
  auto &selectedPoint = cpData->selectedPoint;
  auto &selectedPoints = cpData->selectedPoints;
  auto &cpsAnim = cpData->cpsAnim;
  auto &def = defData->def;
  auto &VCurr = defData->VCurr;

  if (selectedPoint == -1) return;

  for (int cpId : selectedPoints) {
    try {
      auto &cp = def.getCP(cpId);
      if (apply) {
        cp.pos.z() =
            VCurr.row(cp.ptId).z();  // don't allow z-translation: update
                                     // z-coordinate of control point by
                                     // z-coordinate of corresponding mesh point
        cp.prevPos = cp.pos;         // apply
      } else
        cp.pos = cp.prevPos;  // discard
      const auto &it = cpsAnim.find(cpId);
      if (it != cpsAnim.end()) {
        auto &cpAnim = it->second;
        if (apply) {
          // don't allow z-translation: update z-coordinate of control point by
          // z-coordinate of corresponding mesh point
          MatrixXd &T = cpAnim.getTransform();
          T(2, 3) = 0;
          cpAnim.applyTransform();  // apply
        } else
          cpAnim.setTransform(Matrix4d::Identity());  // discard
      }
    } catch (out_of_range &e) {
      cerr << e.what() << endl;
    }
  }
  transformRelative = false;
  repaint = true;
}

void MainWindow::transformApply() { transformEnd(true); }

void MainWindow::transformDiscard() { transformEnd(false); }

void MainWindow::removeControlPointOrRegion() {
  if (manipulationMode.isGeometryModeActive()) {
    // if there are CP animations, remove them only; remove control points
    // otherwise
    if (!removeCPAnims()) removeControlPoints();
  } else {
    if (!removeSelectedLayers()) {
      removeLastRegion();
    }
    recreateMergedImgs();
  }
  repaint = true;
}

void MainWindow::removeControlPoints() {
  auto &selectedPoints = this->cpData.selectedPoints;
  auto &selectedPoint = this->cpData.selectedPoint;
  auto &cpsAnim = this->cpData.cpsAnim;
  auto &cpsAnimSyncId = this->cpData.cpsAnimSyncId;

  // remove selected control points and their animations
  selectedPoint = -1;
  for (auto it = selectedPoints.begin(); it != selectedPoints.end();) {
    const int cpId = *it;
    it = selectedPoints.erase(it);
    def.removeControlPoint(cpId);
    cpsAnim.erase(cpId);
    if (cpId == cpsAnimSyncId) cpsAnimSyncId = -1;
  }
  repaint = true;
}

bool MainWindow::removeSelectedLayers() {
  if (selectedLayer == -1) return false;

  vector<int> regIds;
  for (int layerId : selectedLayers) {
    const int regId =
        layerId >= 0 && layerId < layers.size() ? layers[layerId] : -1;
    if (regId != -1) regIds.push_back(regId);
  }
  for (int regId : regIds) {
    removeRegion(regId);
  }

  selectedLayer = -1;
  selectedLayers.clear();

  return true;
}

void MainWindow::removeLastRegion() {
  // find last region
  int regId = -1;
  for (int i = outlineImgs.size() - 1; i >= 0; i--) {
    if (!outlineImgs[i].isNull()) {
      regId = i;
      break;
    }
  }
  if (regId == -1) return;

  removeRegion(regId);
}

void MainWindow::removeRegion(int regId) {
  if (!(regId >= 0 && regId < regionImgs.size())) return;

  DEBUG_CMD_MM(cout << "removing region; " << regId << endl;)
  outlineImgs[regId].setNull();
  regionImgs[regId].setNull();
  for (auto it = layers.begin(); it != layers.end(); it++) {
    if (*it == regId) {
      layers.erase(it);
      break;
    }
  }
}

void MainWindow::selectAllRegions() {
  forlist(i, layers) selectedLayers.insert(i);
  if (!layers.empty()) selectedLayer = layers[0];
  if (manipulationMode.isImageModeActive()) recreateMergedImgs();
  repaint = true;
}

void MainWindow::deselectAllRegions() {
  selectedLayer = -1;
  selectedLayers.clear();
  if (manipulationMode.isImageModeActive()) recreateMergedImgs();
  repaint = true;
}

void MainWindow::deselectAll() {
  deselectAllControlPoints();
  deselectAllRegions();
  repaint = true;
}

void MainWindow::deselectAllControlPoints() {
  if (transformRelative)
    transformDiscard();
  else
    selectedPoints.clear();
  repaint = true;
}

void MainWindow::selectAll() {
  if (manipulationMode.isImageModeActive()) selectAllRegions();
  if (manipulationMode.isGeometryModeActive()) selectAllControlPoints();
  repaint = true;
}

void MainWindow::selectAllControlPoints() {
  auto &selectedPoints = this->cpData.selectedPoints;
  auto &selectedPoint = this->cpData.selectedPoint;

  selectedPoints.clear();
  for (const auto &it : def.getCPs()) selectedPoints.insert(it.first);
  selectedPoint = *selectedPoints.begin();

  repaint = true;
}

template <typename T>
void computeScaledImage(const T targetW, const T targetH, T &newW, T &newH,
                        T &shiftX, T &shiftY) {
  const float r = newW / static_cast<float>(newH);
  // scale up
  newW = targetW;
  newH = 1.f / r * newW;
  // scale down
  if (newH > targetH) {
    newH = targetH;
    newW = r * newH;
  }
  shiftX = 0, shiftY = 0;
  if (newW < targetW) {
    shiftX = 0.5 * (targetW - newW);
  }
  if (newH < targetH) {
    shiftY = 0.5 * (targetH - newH);
  }
}

void MainWindow::loadTemplateImageFromFile(bool scale) {
  Imguc tmp = Imguc::loadImage("/tmp/template.img", 3, 4);
  DEBUG_CMD_MM(cout << "loadTemplateImageFromFile: " << tmp << endl;);
  if (!tmp.isNull() && tmp.w > 0 && tmp.h > 0) {
    if (scale) {
      int newW = tmp.w, newH = tmp.h;
      int shiftX = 0, shiftY = 0;
      computeScaledImage(viewportW, viewportH, newW, newH, shiftX, shiftY);
      templateImg.setNull() =
          tmp.scale(newW, newH)
              .resize(windowWidth, windowHeight, shiftX, shiftY, Cu{255});
    } else {
      templateImg.setNull() =
          tmp.resize(windowWidth, windowHeight, 0, 0, Cu{255});
    }
    loadTextureToGPU(templateImg, glData.templateImgTexName);
    shadingOpts.showTemplateImg = true;
    shadingOpts.showTexture = true;
  }
  repaint = true;
}

void MainWindow::loadBackgroundImageFromFile(bool scale) {
  Imguc tmp = Imguc::loadImage("/tmp/bg.img", 3, 4);
  DEBUG_CMD_MM(cout << "loadBackgroundImageFromFile: " << tmp << endl;);
  if (!tmp.isNull() && tmp.w > 0 && tmp.h > 0) {
    if (scale) {
      int newW = tmp.w, newH = tmp.h;
      int shiftX = 0, shiftY = 0;
      computeScaledImage(viewportW, viewportH, newW, newH, shiftX, shiftY);
      backgroundImg.setNull() =
          tmp.scale(newW, newH)
              .resize(windowWidth, windowHeight, shiftX, shiftY, Cu{255});
    } else {
      backgroundImg.setNull() =
          tmp.resize(windowWidth, windowHeight, 0, 0, Cu{255});
    }
    loadTextureToGPU(backgroundImg, glData.backgroundImgTexName);
    shadingOpts.showBackgroundImg = true;
  }
  repaint = true;
}

// START OF ANIMATION
void MainWindow::cpRecordingRequestOrCancel() {
  cpData.recordCPWaitForClick = !cpData.recordCPWaitForClick;
  if (cpData.recordCPWaitForClick) {
    DEBUG_CMD_MM(
        cout << "CP recording requested, waiting for click on a control point"
             << endl;)
  } else {
    DEBUG_CMD_MM(cout << "CP recording request canceled" << endl;)

    // if recording, stop it
    if (cpData.recordCP) {
      setRecordingCP(false);
      cpData.recordCPWaitForClick = false;
    }
  }
}

void MainWindow::setRecordingCP(bool active) {
  auto &selectedPoints = this->cpData.selectedPoints;
  auto &cpsAnim = this->cpData.cpsAnim;
  auto &recordCP = this->cpData.recordCP;
  auto &recordCPActive = this->cpData.recordCPActive;
  auto &animMode = cpData.animMode;

  recordCP = active;

  for (int i : selectedPoints) {
    const auto &it = cpsAnim.find(i);
    if (it == cpsAnim.end()) continue;
    auto &cpAnim = it->second;
    if (recordCP) {
      // recording started - clear previously recorded control point animation
      // if it exists
      cpAnim = CPAnim();
    } else {
      // recording stopped
      cpAnim.setAll(false, true);
      if (autoSmoothAnim) {
        int from = autoSmoothAnimFrom;
        int to = autoSmoothAnimTo;
        if (animMode == ANIM_MODE_OVERWRITE || animMode == ANIM_MODE_AVERAGE) {
          int syncLength = cpAnimSync.getLength();
          int syncT = syncLength > 0
                          ? static_cast<int>(cpAnimSync.lastT) % syncLength
                          : 0;
          from = syncT + autoSmoothAnimFrom;
          to = syncT + autoSmoothAnimTo;
        }
        cpAnim.performSmoothing(from, to, autoSmoothAnimIts);
      }
    }
  }
  if (recordCP) {
    // recording started
    recordCPActive = true;
    cpData.timestampStart = chrono::high_resolution_clock::now();
    DEBUG_CMD_MM(cout << "CP recording started" << endl;)
  } else {
    // recording stopped
    // Stop relative transformation when animation recording finished.
    transformEnd(false);
    DEBUG_CMD_MM(cout << "CP recording stopped" << endl;)
  }
}

void MainWindow::cpAnimationPlaybackAndRecord() {
  auto &selectedPoints = this->cpData.selectedPoints;
  auto &selectedPoint = this->cpData.selectedPoint;
  auto &cpsAnim = this->cpData.cpsAnim;
  auto &cpAnimSync = this->cpData.cpAnimSync;
  auto &cpsAnimSyncId = this->cpData.cpsAnimSyncId;
  auto &recordCP = this->cpData.recordCP;
  auto &animMode = cpData.animMode;
  auto &playAnimation = cpData.playAnimation;
  auto &playAnimWhenSelected = cpData.playAnimWhenSelected;

  // control points animation

  // find sync control point
  bool cpAnimSyncUpdating = false;
  if (!cpsAnim.empty()) {
    const auto &it = cpsAnim.find(cpsAnimSyncId);
    if (it == cpsAnim.end()) {
      // sync anim does not exist anymore - create it from the first cpAnim in
      // the list
      cpsAnimSyncId = cpsAnim.begin()->first;
      cpAnimSync =
          CPAnim(cpsAnim.begin()
                     ->second.getKeyposes());  // initialize only with keyposes
                                               // of the first cpAnim
      DEBUG_CMD_MM(cout << "cpsanimfirst " << cpsAnimSyncId << endl;)
    } else {
      if (cpAnimSync.getLength() != it->second.getLength()) {
        // Lengths of sync animation and its assigned animations differ:
        // update cpAnimSync: only by keyposes of the first cpAnim.
        cpAnimSync = CPAnim(it->second.getKeyposes());
      }
    }

    // check if we're updating sync CP anim
    if (selectedPoints.find(cpsAnimSyncId) != selectedPoints.end()) {
      cpAnimSyncUpdating = true;
    }
  } else {
    cpsAnimSyncId = -1;
    cpAnimSync = CPAnim();
  }

  // sync timepoint hack allowing for animation speed-up/slow-down
  if (cpsAnimSyncId != -1 && cpAnimSync.getLength() > 0) {
    if (playAnimation || recordCP) {
      if (!manualTimepoint) {
        cpAnimSync.lastT++;
        if (cpAnimSync.lastT >= 100 * cpAnimSync.getLength())
          cpAnimSync.lastT = 0;
      }
    }
  }

  if (!def.getCPs().empty()) {
    // recording
    if (recordCP && selectedPoint != -1) {
      bool overwriteMode = false;
      int syncLength = 0;
      int syncT = 0;
      if (!cpAnimSyncUpdating &&
          (animMode == ANIM_MODE_OVERWRITE || animMode == ANIM_MODE_AVERAGE)) {
        if (cpsAnimSyncId != -1 && cpAnimSync.getLength() > 0)
          overwriteMode = true;
        if (overwriteMode) {
          // sync CP exists, get its length and timepoint
          syncLength = cpAnimSync.getLength();
          syncT = syncLength > 0
                      ? static_cast<int>(cpAnimSync.lastT) % syncLength
                      : 0;
        }
      }

      for (int cpId : selectedPoints) {
        try {
          Vector3d p = def.getCP(cpId).pos;
          auto &cpAnim = cpsAnim[cpId];
          int timestamp =
              chrono::duration_cast<chrono::milliseconds>(
                  chrono::high_resolution_clock::now() - cpData.timestampStart)
                  .count();

          if (!cpAnimSyncUpdating && overwriteMode) {
            if (cpAnim.getLength() != syncLength) {
              // lengths do not match, recreate, initialize with empty flag
              cpAnim = CPAnim(syncLength, p);
            }
            const auto &kPrev = cpAnim.peekk(syncT);
            // If the value at the current timepoint is empty, set it to the
            // current position, otherwise compute average of previous and
            // current position.
            if (animMode == ANIM_MODE_AVERAGE && !kPrev.empty) {
              p = 0.75 * p + 0.25 * kPrev.p;
            }
            cpAnim.record(syncT,
                          CPAnim::Keypose{p, timestamp, false, true, true});
            const int syncTNext = syncLength > 0 ? (syncT + 1) % syncLength : 0;
            auto &kNext = cpAnim.peekk(syncTNext);
            kNext.display = false;
          } else {
            if (cpAnim.lastT >= 1) {
              auto &k = cpAnim.peekk(static_cast<int>(cpAnim.lastT));
              k.display = true;
            }
            cpAnim.record(CPAnim::Keypose{p, timestamp, true, false, true});
          }
        } catch (out_of_range &e) {
          cerr << e.what() << endl;
        }
      }
    }

    // playback
    if (playAnimation && !manualTimepoint && !cpsAnim.empty()) {
      defEng.solveForZ = false;  // pause z-deformation
    }
    if (playAnimation || recordCP) {
      for (auto &it2 : cpsAnim) {
        const int cpId = it2.first;
        CPAnim &a = it2.second;
        // skip selected points
        if (selectedPoints.find(cpId) != selectedPoints.end() &&
            (!playAnimWhenSelected || recordCP)) {
          continue;
        }
        a.syncSetLength(cpAnimSync.getLength());
        try {
          auto &cp = def.getCP(cpId);
          cp.pos = cp.prevPos = a.replay(cpAnimSync.lastT);
        } catch (out_of_range &e) {
          cerr << e.what() << endl;
        }
      }
    }
  }
}

void MainWindow::setAnimRecMode(AnimMode animMode) {
  cpData.animMode = animMode;
}

AnimMode MainWindow::getAnimRecMode() { return cpData.animMode; }

void MainWindow::toggleAnimationPlayback() {
  cpData.playAnimation = !cpData.playAnimation;
  defEng.solveForZ = !cpData.playAnimation;  // pause z-deformation
  repaint = true;
}

bool MainWindow::isAnimationPlaying() { return cpData.playAnimation; }

void MainWindow::toggleAnimationSelectedPlayback() {
  cpData.playAnimWhenSelected = !cpData.playAnimWhenSelected;
  repaint = true;
}

void MainWindow::offsetSelectedCpAnimsByFrames(double offset) {
  auto &selectedPoints = this->cpData.selectedPoints;
  auto &cpsAnim = this->cpData.cpsAnim;

  for (auto &it : cpsAnim) {
    const int cpId = it.first;
    if (selectedPoints.find(cpId) == selectedPoints.end()) continue;
    auto &cpAnim = it.second;
    cpAnim.setOffset(cpAnim.getOffset() + offset);
    try {
      auto &cp = def.getCP(cpId);
      cp.pos = cp.prevPos = cpAnim.peek();
    } catch (out_of_range &e) {
      cerr << e.what() << endl;
    }
  }

  repaint = true;
}

void MainWindow::offsetSelectedCpAnimsByPercentage(double offset) {
  // TODO: not implemented
  repaint = true;
}

void MainWindow::toggleAutoSmoothAnim() { autoSmoothAnim = !autoSmoothAnim; }

bool MainWindow::copySelectedAnim() {
  auto &selectedPoint = this->cpData.selectedPoint;
  auto &cpsAnim = this->cpData.cpsAnim;

  const auto &it = cpsAnim.find(selectedPoint);
  if (it == cpsAnim.end()) {
    DEBUG_CMD_MM(cout << "No control point animation selected." << endl;);
    return false;
  }
  copiedAnim = it->second;
  return true;
}

bool MainWindow::pasteSelectedAnim() {
  auto &selectedPoints = this->cpData.selectedPoints;
  auto &selectedPoint = this->cpData.selectedPoint;
  auto &cpsAnim = this->cpData.cpsAnim;

  if (selectedPoint == -1) {
    DEBUG_CMD_MM(cout << "No control point selected." << endl;);
    return false;
  }
  for (int cpId : selectedPoints) {
    try {
      auto &cp = def.getCP(cpId);
      auto &cpAnim = cpsAnim[cpId];
      cpAnim = copiedAnim;
      Vector3d t = -cpAnim.peek(0) + cp.pos;
      Affine3d T((Translation3d(t)));
      cpAnim.setTransform(T.matrix());
      cpAnim.applyTransform();
    } catch (out_of_range &e) {
      cerr << e.what() << endl;
    }
  }

  repaint = true;

  return true;
}

bool MainWindow::removeCPAnims() {
  auto &selectedPoints = this->cpData.selectedPoints;
  auto &cpsAnim = this->cpData.cpsAnim;
  auto &cpsAnimSyncId = this->cpData.cpsAnimSyncId;

  // remove only animations of selected control points
  bool cpErased = false;
  for (const int cpId : selectedPoints) {
    if (cpId == cpsAnimSyncId) cpsAnimSyncId = -1;
    if (cpsAnim.erase(cpId) != 0) cpErased = true;
  }

  repaint = true;

  return cpErased;
}
// END OF ANIMATION

void MainWindow::setCPsVisibility(bool visible) {
  cpData.showControlPoints = visible;
  repaint = true;
}

bool MainWindow::getCPsVisibility() { return cpData.showControlPoints; }

ManipulationMode MainWindow::openProject(const std::string &zipFn,
                                         bool changeMode) {
  reset();
  ManipulationMode newManipulationMode;
  loadAllFromZip(zipFn, viewportW, viewportH, cpData, imgData, recData,
                 savedCPs, templateImg, backgroundImg, shadingOpts,
                 newManipulationMode, middleMouseSimulation);
  shadingOpts.showTexture = shadingOpts.showTemplateImg;
  if (!templateImg.isNull()) {
    loadTextureToGPU(templateImg, glData.templateImgTexName);
  }
  if (!backgroundImg.isNull()) {
    loadTextureToGPU(backgroundImg, glData.backgroundImgTexName);
  }
  if (changeMode) changeManipulationMode(newManipulationMode, false);
  if (manipulationMode.isImageModeActive()) recreateMergedImgs();
  enableMiddleMouseSimulation(middleMouseSimulation);

  repaint = true;
  return newManipulationMode;
}

void MainWindow::saveProject(const std::string &zipFn) {
  auto *cpDataAnimateMode = &cpData;
  if (manipulationMode.mode == DEFORM_MODE) cpDataAnimateMode = &cpDataBackup;
  saveAllToZip(zipFn, *cpDataAnimateMode, defData, imgData, recData, savedCPs,
               templateImg, backgroundImg, shadingOpts, manipulationMode,
               middleMouseSimulation);
}

void MainWindow::setTemplateImageVisibility(bool visible) {
  shadingOpts.showTemplateImg = visible;
  shadingOpts.showTexture = visible;
  repaint = true;
}

bool MainWindow::getTemplateImageVisibility() {
  return shadingOpts.showTemplateImg;
}

bool MainWindow::hasTemplateImage() { return !templateImg.isNull(); }

void MainWindow::setBackgroundImageVisibility(bool visible) {
  shadingOpts.showBackgroundImg = visible;
  repaint = true;
}

bool MainWindow::getBackgroundImageVisibility() {
  return shadingOpts.showBackgroundImg;
}

void MainWindow::enableTextureShading(bool enabled) {
  shadingOpts.showTextureUseMatcapShading = enabled;
  repaint = true;
}

bool MainWindow::isTextureShadingEnabled() {
  return shadingOpts.showTextureUseMatcapShading;
}

ManipulationMode &MainWindow::getManipulationMode() { return manipulationMode; }

void MainWindow::recomputeCameraCenter() {
  auto &V = defData.VCurr;

  if (V.rows() == 0) return;

  cameraCenter.fill(0);
  cameraCenter =
      (V.colwise().sum() / V.rows()).transpose();  // model's centroid
  cameraCenter(2) = 0;
  cameraCenterViewSpace =
      (proj3DView * cameraCenter.homogeneous()).hnormalized() -
      Vector3d(translateView.x(), translateView.y(), 0);

  repaint = true;
}

void MainWindow::enableArmpitsStitching(bool enabled) {
  armpitsStitching = enabled;
  repaint = true;
}

bool MainWindow::isArmpitsStitchingEnabled() { return armpitsStitching; }

void MainWindow::enableNormalSmoothing(bool enabled) {
  shadingOpts.useNormalSmoothing = enabled;
  repaint = true;
}

bool MainWindow::isNormalSmoothingEnabled() {
  return shadingOpts.useNormalSmoothing;
}

void MainWindow::exportTextureTemplate(const std::string &fn) {
  const Imguc &Io = mergedOutlinesImg;
  Imguc I;
  I.initImage(mergedOutlinesImg);
  I.fill(255);
  fora(y, 0, I.h) fora(x, 0, I.w) {
    if (Io(x, y, 3) != 0) {
      fora(c, 0, 3) I(x, y, c) = (0.75 * 256 - 1) + 0.25 * Io(x, y, c);
    }
  }

  I.savePNG(fn);
}

void MainWindow::enableMiddleMouseSimulation(bool enabled) {
  middleMouseSimulation = enabled;
  rotationModeActive = enabled;
  repaint = true;
}

bool MainWindow::isMiddleMouseSimulationEnabled() {
  return middleMouseSimulation;
}

void MainWindow::resetView() {
  camData = CameraData();
  recomputeCameraCenter();
  cameraCenterViewSpace = cameraCenter;
  repaint = true;
}

void MainWindow::pauseAll(PauseStatus &status) {
  if (manipulationMode.mode != ANIMATE_MODE) return;
  status.animPaused = !cpData.playAnimation;
  cpData.playAnimation = false;
  defPaused = true;
  repaint = true;
}

void MainWindow::resumeAll(PauseStatus &status) {
  if (manipulationMode.mode != ANIMATE_MODE) return;
  cpData.playAnimation = !status.animPaused;
  defPaused = false;
  repaint = true;
}

void MainWindow::exportAsOBJ(const std::string &outDir,
                             const std::string &outFnWithoutExtension,
                             bool saveTexture) {
  MatrixXd V = defData.VCurr;
  MatrixXd N = defData.normals;
  V *= 10.0 / viewportW;

  V.array().rowwise() *= RowVector3d(1, -1, -1).array();
  N.array().rowwise() *= RowVector3d(1, -1, -1).array();
  V.rowwise() += RowVector3d(-5, 5, 0);

  MatrixXd textureCoords;
  if (!templateImg.isNull() && saveTexture) {
    textureCoords = (defData.VRestOrig.array().rowwise() /
                     Array3d(templateImg.w, -templateImg.h, 1).transpose());
  }
  string objFn = outDir + "/" + outFnWithoutExtension + ".obj";
  writeOBJ(objFn, V, defData.Faces, N, defData.Faces, textureCoords,
           defData.Faces);
  if (!templateImg.isNull() && saveTexture) {
    // write material to a file
    {
      ofstream stream(objFn, ofstream::out | ofstream::app);
      if (stream.is_open()) {
        stream << "s 1" << endl;
        stream << "mtllib " << outFnWithoutExtension << ".mtl" << endl;
        stream << "usemtl Textured" << endl;
        stream.close();
      }
    }
    {
      ofstream stream(outDir + "/" + outFnWithoutExtension + ".mtl");
      if (stream.is_open()) {
        stream << "newmtl Textured\nKa 1.000 1.000 1.000\nKd 1.000 1.000 "
                  "1.000\nKs 0.000 0.000 0.000\nNs 10.000\nd 1.0\nTr "
                  "0.0\nillum 2\nmap_Ka "
               << outFnWithoutExtension << ".png"
               << "\nmap_Kd " << outFnWithoutExtension << ".png" << endl;
        stream.close();
      }
    }
    templateImg.savePNG(outDir + "/" + outFnWithoutExtension + ".png");
  }
}

void MainWindow::exportAnimationStart(int preroll, bool solveForZ,
                                      bool perFrameNormals) {
  const int nFrames = cpAnimSync.getLength();
  manualTimepoint = true;
  if (nFrames > 0) {
    cpAnimSync.lastT = (100 * nFrames - preroll) % nFrames;
  }
  cpData.playAnimation = true;
  defEng.solveForZ = solveForZ;  // resume z-deformation
  defPaused = false;

  if (gltfExporter != nullptr) delete gltfExporter;
  gltfExporter = new exportgltf::ExportGltf;
  exportAnimationWaitForBeginning = true;
  exportAnimationPreroll = preroll;
  exportPerFrameNormals = perFrameNormals;
  exportedFrames = 0;

  repaint = true;

  DEBUG_CMD_MM(cout << "exportAnimationStart" << endl;);
}

void MainWindow::exportAnimationStop(bool exportModel) {
  manualTimepoint = false;
  defEng.solveForZ = false;  // pause z-deformation
  cpData.playAnimation = false;
  defPaused = true;

  if (gltfExporter != nullptr) {
    if (exportModel) {
      gltfExporter->exportStop("/tmp/mm_project.glb", true);
#ifdef __EMSCRIPTEN__
      EM_ASM(js_exportAnimationFinished(););
#endif
    }
    delete gltfExporter;
    gltfExporter = nullptr;
  }

  repaint = true;

  DEBUG_CMD_MM(cout << "exportAnimationStop" << endl;);
}

void MainWindow::exportAnimationFrame() {
  if (gltfExporter == nullptr) {
    DEBUG_CMD_MM(cerr << "exportAnimationFrame: gltfModel == nullptr" << endl;);
    return;
  }

  const int nFrames = cpAnimSync.getLength();
  bool exportSingleFrame = nFrames == 0;

  int prerollCurr = 0;
  if (!exportSingleFrame) {
    const int prerollStart = (100 * nFrames - exportAnimationPreroll) % nFrames;
    prerollCurr = static_cast<int>(cpAnimSync.lastT) - prerollStart;
    if (exportAnimationWaitForBeginning) {
      if (prerollCurr >= exportAnimationPreroll) {
        exportAnimationWaitForBeginning = false;
      } else {
        DEBUG_CMD_MM(cout << "exportAnimationFrame: waiting for beginning ("
                          << cpAnimSync.lastT << ")" << endl;);
      }
    }
  } else {
    exportAnimationWaitForBeginning = false;
  }

  if (!exportAnimationWaitForBeginning) {
    prerollCurr = exportAnimationPreroll;
    DEBUG_CMD_MM(cout << "exportAnimationFrame: " << exportedFrames << endl;);
    exportgltf::MatrixXfR V = defData.VCurr.cast<float>();
    V *= 10.0 / viewportW;
    V.array().rowwise() *= RowVector3f(1, -1, -1).array();
    V.rowwise() += RowVector3f(-5, 5, 0);

    const int nFrames = cpAnimSync.getLength();
    const bool hasTexture = !templateImg.isNull();

    exportgltf::MatrixXfR N;
    if (exportedFrames == 0 || exportPerFrameNormals) {
      N = defData.normals.cast<float>();
      N.array().rowwise() *= RowVector3f(1, -1, -1).array();
    }
    if (exportedFrames == 0) {
      exportBaseV = V;
      exportBaseN = N;
      exportgltf::MatrixXusR F = defData.Faces.cast<unsigned short>();

      exportgltf::MatrixXfR TC;
      if (hasTexture) {
        TC = (defData.VRestOrig.leftCols(2).cast<float>().array().rowwise() /
              Array2f(templateImg.w, templateImg.h).transpose());
      }

      gltfExporter->exportStart(V, N, F, TC, nFrames, exportPerFrameNormals, 24,
                                templateImg);
      gltfExporter->exportFullModel(V, N, F, TC);
    } else {
      V -= exportBaseV;
      if (exportPerFrameNormals) N -= exportBaseN;
      gltfExporter->exportMorphTarget(V, N, exportedFrames);
    }
    exportedFrames++;
  }

  // update progress bar
  int progress = 100;
  if (!exportSingleFrame) {
    progress = round(100.0 * (prerollCurr + exportedFrames) /
                     (nFrames + exportAnimationPreroll));
  }
#ifdef __EMSCRIPTEN__
  EM_ASM({ js_exportAnimationProgress($0); }, progress);
#endif

  cpAnimSync.lastT++;
  if (cpAnimSync.lastT >= 100 * nFrames) cpAnimSync.lastT = 0;

  if (!exportAnimationWaitForBeginning) {
    if (exportSingleFrame ||
        (nFrames > 0 && (static_cast<int>(cpAnimSync.lastT) % nFrames == 0))) {
      // reached the end of animation
      exportAnimationStop();
      return;
    }
  }

  repaint = true;
}

bool MainWindow::exportAnimationRunning() { return gltfExporter != nullptr; }

void MainWindow::pauseAnimation() { pauseAll(animStatus); }
void MainWindow::resumeAnimation() { resumeAll(animStatus); }

int MainWindow::getNumberOfAnimationFrames() { return cpAnimSync.getLength(); }
