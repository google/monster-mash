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

#include <iostream>

#include "mainwindow.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

using namespace std;

MainWindow mainWindow(
    1000, 800, "Monster Mash: New Sketch-Based Modeling and Animation Tool");

#ifdef __EMSCRIPTEN__
extern "C" {

EMSCRIPTEN_KEEPALIVE void selectAll() { mainWindow.selectAll(); }

EMSCRIPTEN_KEEPALIVE void deselectAll() { mainWindow.deselectAll(); }

EMSCRIPTEN_KEEPALIVE void copySelectedAnim() { mainWindow.copySelectedAnim(); }

EMSCRIPTEN_KEEPALIVE void pasteSelectedAnim() {
  mainWindow.pasteSelectedAnim();
}

EMSCRIPTEN_KEEPALIVE void offsetSelectedCpAnimsByFrames(double offset) {
  mainWindow.offsetSelectedCpAnimsByFrames(offset);
}

EMSCRIPTEN_KEEPALIVE void loadBackgroundImage() {
  mainWindow.loadBackgroundImageFromFile();
}

EMSCRIPTEN_KEEPALIVE void setBackgroundImageVisibility(bool visible) {
  mainWindow.setBackgroundImageVisibility(visible);
}

EMSCRIPTEN_KEEPALIVE bool getBackgroundImageVisibility() {
  return mainWindow.getBackgroundImageVisibility();
}

EMSCRIPTEN_KEEPALIVE void loadTemplateImage() {
  mainWindow.loadTemplateImageFromFile();
}

EMSCRIPTEN_KEEPALIVE void setTemplateImageVisibility(bool visible) {
  mainWindow.setTemplateImageVisibility(visible);
}

EMSCRIPTEN_KEEPALIVE bool getTemplateImageVisibility() {
  return mainWindow.getTemplateImageVisibility();
}

EMSCRIPTEN_KEEPALIVE bool hasTemplateImage() {
  return mainWindow.hasTemplateImage();
}

EMSCRIPTEN_KEEPALIVE void enableTextureShading(bool enabled) {
  mainWindow.enableTextureShading(enabled);
}

EMSCRIPTEN_KEEPALIVE bool isTextureShadingEnabled() {
  return mainWindow.isTextureShadingEnabled();
}

EMSCRIPTEN_KEEPALIVE void setCPsVisibility(bool visible) {
  mainWindow.setCPsVisibility(visible);
}

EMSCRIPTEN_KEEPALIVE bool getCPsVisibility() {
  return mainWindow.getCPsVisibility();
}

EMSCRIPTEN_KEEPALIVE bool isAnimationPlaying() {
  return mainWindow.isAnimationPlaying();
}

EMSCRIPTEN_KEEPALIVE void setAnimRecMode(int animModeInt) {
  AnimMode animMode = ANIM_MODE_TIME_SCALED;
  if (animModeInt != 0) animMode = ANIM_MODE_OVERWRITE;
  mainWindow.setAnimRecMode(animMode);
}

EMSCRIPTEN_KEEPALIVE int getAnimRecMode() {
  AnimMode mode = mainWindow.getAnimRecMode();
  int modeInt = 0;
  if (mode == ANIM_MODE_OVERWRITE) modeInt = 1;
  return modeInt;
}

EMSCRIPTEN_KEEPALIVE void cpRecordingRequestOrCancel() {
  mainWindow.cpRecordingRequestOrCancel();
}

EMSCRIPTEN_KEEPALIVE void toggleAnimationPlayback() {
  mainWindow.toggleAnimationPlayback();
}

EMSCRIPTEN_KEEPALIVE void removeControlPointOrRegion() {
  mainWindow.removeControlPointOrRegion();
}

EMSCRIPTEN_KEEPALIVE void reset() { mainWindow.reset(); }

int convertManipulationModeToInt(ManipulationMode &mode) {
  int modeInt = 0;
  if (mode.mode == REGION_REDRAW_MODE)
    modeInt = 1;
  else if (mode.mode == DEFORM_MODE)
    modeInt = 2;
  else if (mode.mode == ANIMATE_MODE)
    modeInt = 3;
  return modeInt;
}

EMSCRIPTEN_KEEPALIVE int getManipulationMode() {
  ManipulationMode &mode = mainWindow.getManipulationMode();
  return convertManipulationModeToInt(mode);
}

EMSCRIPTEN_KEEPALIVE void setManipulationMode(int modeInt) {
  ManipulationMode mode = DRAW_OUTLINE;
  if (modeInt == 1)
    mode = REGION_REDRAW_MODE;
  else if (modeInt == 2)
    mode = DEFORM_MODE;
  else if (modeInt == 3)
    mode = ANIMATE_MODE;
  mainWindow.changeManipulationMode(mode);
}

EMSCRIPTEN_KEEPALIVE void enableArmpitsStitching(bool enabled) {
  mainWindow.enableArmpitsStitching(enabled);
}

EMSCRIPTEN_KEEPALIVE bool isArmpitsStitchingEnabled() {
  return mainWindow.isArmpitsStitchingEnabled();
}

EMSCRIPTEN_KEEPALIVE void enableNormalSmoothing(bool enabled) {
  mainWindow.enableNormalSmoothing(enabled);
}

EMSCRIPTEN_KEEPALIVE bool isNormalSmoothingEnabled() {
  return mainWindow.isNormalSmoothingEnabled();
}

EMSCRIPTEN_KEEPALIVE void enableMiddleMouseSimulation(bool enabled) {
  mainWindow.enableMiddleMouseSimulation(enabled);
}

EMSCRIPTEN_KEEPALIVE bool isMiddleMouseSimulationEnabled() {
  return mainWindow.isMiddleMouseSimulationEnabled();
}

EMSCRIPTEN_KEEPALIVE int getVersion() { return APP_VERSION; }

EMSCRIPTEN_KEEPALIVE void openProject() {
  int prevMode = getManipulationMode();
  ManipulationMode mode =
      mainWindow.openProject("/tmp/projectOpened.zip", false);
  int newMode = convertManipulationModeToInt(mode);
  EM_ASM(js_projectOpened(););
  //  if (prevMode != newMode) {
  EM_ASM({ js_manipulationModeChanged($0); }, newMode);
  //  }
}

EMSCRIPTEN_KEEPALIVE void saveProject() {
  mainWindow.saveProject("/tmp/mm_project.zip");
  EM_ASM(js_projectSaved(););
}

EMSCRIPTEN_KEEPALIVE void exportTextureTemplate() {
  mainWindow.exportTextureTemplate("/tmp/mm_template.png");
  EM_ASM(js_textureTemplateExported(););
}

EMSCRIPTEN_KEEPALIVE void resetView() { mainWindow.resetView(); }

EMSCRIPTEN_KEEPALIVE void openExampleProject(unsigned int id) {
  if (id > 8) return;
  int prevMode = getManipulationMode();
  char examples[8][20] = {
      "antelope",         "bird",  "box",       "helene-dino",
      "helene-wienerdog", "heart", "chihuahua", "helene-tree",
  };
  char fn[255];
  sprintf(fn, "/tmp/examples/%s.zip", examples[id]);
  ManipulationMode mode = mainWindow.openProject(fn, false);
  int newMode = convertManipulationModeToInt(mode);
  EM_ASM(js_projectOpened(););
  EM_ASM({ js_manipulationModeChanged($0); }, newMode);
}

EMSCRIPTEN_KEEPALIVE void exportAsOBJ() {
  mainWindow.exportAsOBJ("/tmp", "mm_frame", true);
  EM_ASM(js_frameExportedToOBJ(););
}

EMSCRIPTEN_KEEPALIVE void exportAnimationStart(int preroll, bool solveForZ,
                                               bool perFrameNormals) {
  if (!mainWindow.exportAnimationRunning()) {
    mainWindow.exportAnimationStart(preroll, solveForZ, perFrameNormals);
  } else {
    mainWindow.exportAnimationStop();
  }
}

EMSCRIPTEN_KEEPALIVE void exportAnimationAbort() {
  if (mainWindow.exportAnimationRunning()) {
    mainWindow.exportAnimationStop(false);
  }
}

EMSCRIPTEN_KEEPALIVE bool exportAnimationRunning() {
  return mainWindow.exportAnimationRunning();
}

EMSCRIPTEN_KEEPALIVE void pauseAnimation() { mainWindow.pauseAnimation(); }

EMSCRIPTEN_KEEPALIVE void resumeAnimation() { mainWindow.resumeAnimation(); }

EMSCRIPTEN_KEEPALIVE int getNumberOfAnimationFrames() {
  return mainWindow.getNumberOfAnimationFrames();
}

EMSCRIPTEN_KEEPALIVE void enableKeyboardEvents() {
  mainWindow.enableKeyboardEvents();
}

EMSCRIPTEN_KEEPALIVE void disableKeyboardEvents() {
  mainWindow.disableKeyboardEvents();
}
}
#endif

int main(int argc, char *argv[]) {
  mainWindow.runLoop();
  return 0;
}
