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

#ifndef MYPAINTER_H
#define MYPAINTER_H

#include <SDL.h>
#include <image/image.h>

#include <iostream>
#include <string>
#include <unordered_map>

#include "mywindow.h"
#define GL_GLEXT_PROTOTYPES 1
#include <SDL_opengles2.h>

class MyPainter {
 public:
  MyPainter();
  MyPainter(MyWindow *myWindow);
  MyPainter(Imguc &I);
  virtual ~MyPainter();

  void setRenderer(SDL_Renderer *renderer);
  void setWindow(SDL_Window *window);

  void setColor(unsigned char r, unsigned char g, unsigned char b,
                unsigned char a);
  void setColor(const Cu &c);
  void setThickness(int thickness);
  void drawEllipse(int x, int y, int rx, int ry);
  void filledEllipse(int x, int y, int rx, int ry);
  void drawLine(int x1, int y1, int x2, int y2, int thickness);
  void drawLine(int x1, int y1, int x2, int y2);
  void drawRect(int x1, int y1, int x2, int y2);
  void drawArrow(double x1, double y1, double x2, double y2,
                 double arrowHeadSize, double alphaDeg, int thickness,
                 bool headEnd = true, bool headStart = false);
  void drawArrow(double x1, double y1, double x2, double y2,
                 double arrowHeadSize, double alphaDeg, bool headEnd = true,
                 bool headStart = false);
  void drawTexture(GLuint textureName, float alpha = 1.f);
  void drawImage(int x, int y, const Imguc &I, float alpha = 1.f);
  void drawStaticImage(int x, int y, const Imguc &I, float alpha = 1.f);
  void drawDynamicImage(int x, int y, const Imguc &I);
  void paint();
  void clearToColor();

  int getCurrentThickness();

 private:
  bool checkRenderingContext();

  MyWindow *myWindow = nullptr;
  SDL_Renderer *renderer = nullptr;
  SDL_Window *window = nullptr;
  SDL_Surface *surface = nullptr;
  SDL_Color currColor;
  int currThickness = 1;
  bool useOpenGLForDrawingInsteadOfSDL = false;
};

#endif  // MYPAINTER_H
