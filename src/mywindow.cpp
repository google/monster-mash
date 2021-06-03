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

#include "mywindow.h"

#include <miscutils/opengltools.h>

#include <iostream>

#include "macros.h"

using namespace std;
using namespace Eigen;

MyWindow::MyWindow(int w, int h, const std::string& windowTitle)
    : windowWidth(w), windowHeight(h) {
  init(windowTitle);
}

MyWindow::~MyWindow() { destroy(); }

int MyWindow::init(const std::string& windowTitle) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    cerr << "SDL_Init Error: " << SDL_GetError() << endl;
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  window = SDL_CreateWindow(windowTitle.c_str(), SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  context = SDL_GL_CreateContext(window);
  if (SDL_GL_SetSwapInterval(-1) == -1) {
    SDL_GL_SetSwapInterval(1);
  }
  cout << "GL version: " << glGetString(GL_VERSION) << endl;
  if (window == nullptr) {
    cerr << "SDL_CreateWindow error: " << SDL_GetError() << endl;
    return 1;
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (renderer == nullptr) {
    cerr << "SDL_CreateRenderer error: " << SDL_GetError() << endl;
    return 1;
  }

  initOpenGLBuffers();

  return 0;
}

void MyWindow::initOpenGLBuffers() {
  // create shaders
  const string vertexShaderSrc =
      "#version 100\n"
      "attribute vec3 position;\n"
      "attribute vec2 texCoord;\n"
      "varying vec2 fragTexCoord;\n"
      "void main() {\n"
      "  fragTexCoord = texCoord;\n"
      "  gl_Position = vec4(position,1);\n"
      "}\n";
  const string fragmentShaderSrc =
      "#version 100\n"
      "precision mediump float;\n"
      "uniform sampler2D tex;\n"
      "uniform float alpha;\n"
      "varying vec2 fragTexCoord;\n"
      "void main() {\n"
      "  vec4 sample = texture2D(tex, fragTexCoord);\n"
      "  if (alpha < 1.0) sample.w = sample.w * alpha;\n"
      "  gl_FragColor = sample;\n"
      "}\n";
  glData.screenShader =
      loadShaders(vertexShaderSrc.c_str(), fragmentShaderSrc.c_str());

  // init buffers for meshes
  glGenVertexArraysOES(1, &glData.VAO);  // extension for OpenGL ES 2.0
  glGenBuffers(1, &glData.VBO_V);
  glGenBuffers(1, &glData.VBO_T);
  glGenBuffers(1, &glData.VBO_F);
  glData.V.resize(4, 3);
  glData.V << -1, 1, 0, 1, 1, 0, 1, -1, 0, -1, -1, 0;
  glData.F.resize(2, 3);
  glData.F << 0, 1, 2, 2, 3, 0;
  glData.T.resize(4, 2);
  glData.T << 0, 0, 1, 0, 1, 1, 0, 1;
  // fill buffers
  glBindVertexArrayOES(glData.VAO);  // extension for OpenGL ES 2.0
  // position
  GLint id;
  glBindBuffer(GL_ARRAY_BUFFER, glData.VBO_V);
  glBufferData(GL_ARRAY_BUFFER, glData.V.size() * sizeof(float),
               glData.V.data(), GL_STATIC_DRAW);
  id = glGetAttribLocation(glData.screenShader, "position");
  glVertexAttribPointer(id, glData.V.cols(), GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(id);
  // texture coords
  glBindBuffer(GL_ARRAY_BUFFER, glData.VBO_T);
  glBufferData(GL_ARRAY_BUFFER, glData.T.size() * sizeof(float),
               glData.T.data(), GL_STATIC_DRAW);
  id = glGetAttribLocation(glData.screenShader, "texCoord");
  glVertexAttribPointer(id, glData.T.cols(), GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(id);
  // faces
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glData.VBO_F);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, glData.F.size() * sizeof(unsigned int),
               glData.F.data(), GL_STATIC_DRAW);

  // init texture
  glGenTextures(1, &glData.screenTexture);
  glBindTexture(GL_TEXTURE_2D, glData.screenTexture);
  Imguc tmp(windowWidth, windowHeight, 4, 3);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, windowWidth, windowHeight, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, tmp.data);
  // set the texture wrapping/filtering options (on the currently bound texture
  // object)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void MyWindow::destroyOpenGLBuffers() {
  // destroy buffers
  glDeleteVertexArraysOES(1, &glData.VAO);
  glDeleteBuffers(1, &glData.VBO_V);
  glDeleteBuffers(1, &glData.VBO_T);
  glDeleteBuffers(1, &glData.VBO_F);
}

#ifdef __EMSCRIPTEN__
void MyWindow::mainTickEmscripten(void* data) {
  static_cast<MyWindow*>(data)->mainTick();
}
#endif

void MyWindow::runLoop() {
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(mainTickEmscripten, (void*)this, -1, 1);
#else
  while (true) {
    if (mainTick()) break;
  }
#endif
}

void MyWindow::destroy() {
  destroyOpenGLBuffers();
  SDL_DestroyRenderer(renderer);
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

bool MyWindow::mainTick() {
  SDL_Event event;

  int lastTimestamp = event.motion.timestamp;
  bool quit = false;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT:
        quit = true;
        break;
      case SDL_KEYUP:
      case SDL_KEYDOWN: {
        MyKeyEvent keyEvent;
        if (event.key.keysym.mod & KMOD_SHIFT) keyEvent.shiftModifier = true;
        if (event.key.keysym.mod & KMOD_CTRL) keyEvent.ctrlModifier = true;
        if (event.key.keysym.mod & KMOD_ALT) keyEvent.altModifier = true;
        if (event.key.keysym.mod & KMOD_NONE) keyEvent.noModifier = true;
        keyEvent.key = event.key.keysym.sym;
        lastKeyEvent = keyEvent;  // store this for usage in mouse events
        if (event.type == SDL_KEYDOWN)
          keyPressEvent(keyEvent);
        else
          keyReleaseEvent(keyEvent);

        this->keyEvent(keyEvent);
      } break;
      case SDL_MOUSEMOTION:
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP: {
        if (!simulateMouseEventByTouch &&
            event.button.which == SDL_TOUCH_MOUSEID)
          break;
        MyMouseEvent mouseEvent;
        mouseEvent.pos = Vector2d(event.button.x, event.button.y);
        mouseEvent.shiftModifier = lastKeyEvent.shiftModifier;
        mouseEvent.ctrlModifier = lastKeyEvent.ctrlModifier;
        mouseEvent.altModifier = lastKeyEvent.altModifier;
        mouseEvent.noModifier = lastKeyEvent.noModifier;

        if (event.type == SDL_MOUSEMOTION) {
          mouseEvent.leftButton = event.motion.state & SDL_BUTTON_LMASK;
          mouseEvent.middleButton = event.motion.state & SDL_BUTTON_MMASK;
          mouseEvent.rightButton = event.motion.state & SDL_BUTTON_RMASK;
          //            mouseEvent.rightButton = false; // temporary
        } else {
          mouseEvent.leftButton = event.button.button == SDL_BUTTON_LEFT;
          mouseEvent.middleButton = event.button.button == SDL_BUTTON_MIDDLE;
          mouseEvent.rightButton = event.button.button == SDL_BUTTON_RIGHT;
          //            mouseEvent.rightButton = false; // temporary
          mouseEvent.numClicks = event.button.clicks;
        }

        if (event.type == SDL_MOUSEBUTTONDOWN)
          lastPressTimestamp = event.button.timestamp;
        else if (event.type == SDL_MOUSEBUTTONUP)
          mouseEvent.pressReleaseDurationMs =
              event.button.timestamp - lastPressTimestamp;

        if (event.type == SDL_MOUSEMOTION)
          mouseMoveEvent(mouseEvent);
        else if (event.type == SDL_MOUSEBUTTONDOWN)
          mousePressEvent(mouseEvent);
        else
          mouseReleaseEvent(mouseEvent);

        this->mouseEvent(mouseEvent);
      } break;
      case SDL_FINGERMOTION:
      case SDL_FINGERDOWN:
      case SDL_FINGERUP: {
        MyFingerEvent fingerEvent;
        // event.tfinger.x and event.tfinger.y are normalized to [0, 1]
        fingerEvent.pos = Vector2d(event.tfinger.x * windowWidth,
                                   event.tfinger.y * windowHeight);
        fingerEvent.fingerId = event.tfinger.fingerId;

        if (event.tfinger.type == SDL_FINGERDOWN)
          currFingerIds.insert(fingerEvent.fingerId);
        else if (event.tfinger.type == SDL_FINGERUP)
          currFingerIds.erase(fingerEvent.fingerId);

        fingerEvent.numFingers = currFingerIds.size();

        if (event.tfinger.type == SDL_FINGERDOWN)
          fingerPressEvent(fingerEvent);
        else if (event.tfinger.type == SDL_FINGERUP)
          fingerReleaseEvent(fingerEvent);
        else
          fingerMoveEvent(fingerEvent);

        this->fingerEvent(fingerEvent);
      } break;
    }
  }

  if (paintEvent()) {
    SDL_GL_SwapWindow(window);
  }

  return quit;
}

SDL_Renderer* MyWindow::getRenderer() const { return renderer; }

SDL_Window* MyWindow::getWindow() const { return window; }

const MyWindowGLData& MyWindow::getGLData() const { return glData; }

void MyWindow::setMouseEventsSimulationByTouch(bool enable) {
  simulateMouseEventByTouch = enable;
}

int MyWindow::getWidth() const { return windowWidth; }

int MyWindow::getHeight() const { return windowHeight; }

void MyWindow::setKeyboardEventState(bool enabled) {
  auto state = enabled ? SDL_ENABLE : SDL_DISABLE;
  SDL_EventState(SDL_TEXTINPUT, state);
  SDL_EventState(SDL_KEYDOWN, state);
  SDL_EventState(SDL_KEYUP, state);
}

void MyWindow::enableKeyboardEvents() { setKeyboardEventState(true); }

void MyWindow::disableKeyboardEvents() { setKeyboardEventState(false); }
