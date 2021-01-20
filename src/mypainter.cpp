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

#include "mypainter.h"

#include <SDL2_gfxPrimitives-mod.h>

#include <iostream>

using namespace std;

MyPainter::MyPainter() {
#ifdef USE_OPENGL_FOR_DRAWING_INSTEAD_OF_SDL
  useOpenGLForDrawingInsteadOfSDL = true;
#endif
}

MyPainter::MyPainter(MyWindow *myWindow) : myWindow(myWindow) {
  renderer = myWindow->getRenderer();
  window = myWindow->getWindow();
#ifdef USE_OPENGL_FOR_DRAWING_INSTEAD_OF_SDL
  useOpenGLForDrawingInsteadOfSDL = true;
#endif
}

MyPainter::MyPainter(Imguc &I) {
  Uint32 format;
  if (I.ch == 4)
    format = SDL_PIXELFORMAT_ABGR8888;
  else if (I.ch == 1)
    format = SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED8, SDL_PACKEDORDER_XRGB,
                                    SDL_PACKEDLAYOUT_8888, 8,
                                    1);  // is this correct?
  else
    assert(false &&
           "MyPainter::MyPainter(Imguc &I): error: unsupported number of "
           "channels");

  surface = SDL_CreateRGBSurfaceWithFormatFrom(
      I.data, I.w, I.h, I.ch * 8, sizeof(unsigned char) * I.w * I.ch, format);
  renderer = SDL_CreateSoftwareRenderer(surface);
}

MyPainter::~MyPainter() {
  // destroy resources used for drawing to image
  if (surface != nullptr) {
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
  }
}

bool MyPainter::checkRenderingContext() {
  if (useOpenGLForDrawingInsteadOfSDL) {
    return window != nullptr;
  } else {
    return renderer != nullptr;
  }
}

void MyPainter::setRenderer(SDL_Renderer *renderer) {
  this->renderer = renderer;
}

void MyPainter::setWindow(SDL_Window *window) { this->window = window; }

void MyPainter::setColor(const Cu &c) {
  currColor.r = c(0);
  currColor.g = c(1);
  currColor.b = c(2);
  currColor.a = c(3);
}

void MyPainter::setColor(unsigned char r, unsigned char g, unsigned char b,
                         unsigned char a) {
  currColor.r = r;
  currColor.g = g;
  currColor.b = b;
  currColor.a = a;
}

void MyPainter::setThickness(int thickness) { currThickness = thickness; }

void MyPainter::drawEllipse(int x, int y, int rx, int ry) {
  if (!checkRenderingContext()) return;
  aaellipseRGBA(renderer, x, y, rx, ry, currColor.r, currColor.g, currColor.b,
                currColor.a);
}

void MyPainter::filledEllipse(int x, int y, int rx, int ry) {
  if (!checkRenderingContext()) return;
  aaFilledEllipseRGBA(renderer, x, y, rx, ry, currColor.r, currColor.g,
                      currColor.b, currColor.a);
}

void MyPainter::drawLine(int x1, int y1, int x2, int y2, int thickness) {
  if (!checkRenderingContext()) return;
  if ((x1 == x2) && (y1 == y2)) {
    const int t = ceil(0.5 * thickness);
    // there is a bug in filledEllipseRGBA; it doesn't draw perfect circles;
    // compensate for it by t-1, which works for small circles
    filledEllipseRGBA(renderer, x1, y1, t - 1, t, currColor.r, currColor.g,
                      currColor.b, currColor.a);
  } else {
    thickLineRGBA(renderer, x1, y1, x2, y2, thickness, currColor.r, currColor.g,
                  currColor.b, currColor.a);
  }
}

void MyPainter::drawLine(int x1, int y1, int x2, int y2) {
  drawLine(x1, y1, x2, y2, currThickness);
}

void MyPainter::drawRect(int x1, int y1, int x2, int y2) {
  rectangleRGBA(renderer, x1, y1, x2, y2, currColor.r, currColor.g, currColor.b,
                currColor.a);
}

void MyPainter::drawArrow(double x1, double y1, double x2, double y2,
                          double arrowHeadSize, double alphaDeg, int thickness,
                          bool headEnd, bool headStart) {
  const double alpha = alphaDeg / 180.0 * M_PI;
  drawLine(x1, y1, x2, y2, thickness);
  const double beta = atan2(y1 - y2, x1 - x2);
  const double x = arrowHeadSize;
  const double ya = tan(alpha) * x;
  const double yb = tan(-alpha) * x;
  if (headEnd) {
    drawLine(x2, y2, x2 + x * cos(beta) - ya * sin(beta),
             y2 + x * sin(beta) + ya * cos(beta), thickness);
    drawLine(x2, y2, x2 + x * cos(beta) - yb * sin(beta),
             y2 + x * sin(beta) + yb * cos(beta), thickness);
  }
  if (headStart) {
    drawLine(x1, y1, x1 - x * cos(beta) + ya * sin(beta),
             y1 - x * sin(beta) - ya * cos(beta), thickness);
    drawLine(x1, y1, x1 - x * cos(beta) + yb * sin(beta),
             y1 - x * sin(beta) - yb * cos(beta), thickness);
  }
}

void MyPainter::drawArrow(double x1, double y1, double x2, double y2,
                          double arrowHeadSize, double alphaDeg, bool headEnd,
                          bool headStart) {
  drawArrow(x1, y1, x2, y2, arrowHeadSize, alphaDeg, currThickness, headEnd,
            headStart);
}

void MyPainter::drawTexture(GLuint textureName, float alpha) {
  auto &glData = myWindow->getGLData();
  glUseProgram(glData.screenShader);
  glUniform1f(glGetUniformLocation(glData.screenShader, "alpha"), alpha);
  // activate texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textureName);
  // draw
  glBindVertexArrayOES(glData.VAO);
  glDisable(GL_DEPTH_TEST);
  glDrawElements(GL_TRIANGLES, 3 * glData.F.rows(), GL_UNSIGNED_INT, 0);
  glEnable(GL_DEPTH_TEST);
}

void MyPainter::drawImage(int x, int y, const Imguc &I, float alpha) {
  if (useOpenGLForDrawingInsteadOfSDL) {
    if (I.ch != 4) {
      cerr << "MyPainter::drawImage: image must be RGBA" << endl;
      return;
    }
    auto &glData = myWindow->getGLData();
    glUseProgram(glData.screenShader);
    glUniform1f(glGetUniformLocation(glData.screenShader, "alpha"), alpha);
    // upload texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glData.screenTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, I.w, I.h, GL_RGBA, GL_UNSIGNED_BYTE,
                    I.data);
    // draw
    glBindVertexArrayOES(glData.VAO);
    glDisable(GL_DEPTH_TEST);
    glDrawElements(GL_TRIANGLES, 3 * glData.F.rows(), GL_UNSIGNED_INT, 0);
    glEnable(GL_DEPTH_TEST);
  } else {
    drawStaticImage(x, y, I, alpha);
  }
}

void MyPainter::drawStaticImage(int x, int y, const Imguc &I, float alpha) {
  if (!checkRenderingContext()) return;

  if (useOpenGLForDrawingInsteadOfSDL) {
  } else {
    SDL_PixelFormatEnum format = SDL_PIXELFORMAT_ABGR8888;
    if (I.ch == 3) format = SDL_PIXELFORMAT_BGR888;
    SDL_Texture *texture =
        SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_STATIC, I.w, I.h);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    if (alpha < 1) SDL_SetTextureAlphaMod(texture, alpha);
    SDL_UpdateTexture(texture, nullptr, I.data,
                      sizeof(unsigned char) * I.w * I.ch);
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = I.w;
    rect.h = I.h;
    SDL_RenderCopy(renderer, texture, nullptr, &rect);
    SDL_DestroyTexture(texture);
  }
}

void MyPainter::drawDynamicImage(int x, int y, const Imguc &I) {
  if (useOpenGLForDrawingInsteadOfSDL) {
  } else {
    if (!checkRenderingContext()) return;
    SDL_PixelFormatEnum format = SDL_PIXELFORMAT_ABGR8888;
    if (I.ch == 3) format = SDL_PIXELFORMAT_BGR888;
    SDL_Texture *texture = SDL_CreateTexture(
        renderer, format, SDL_TEXTUREACCESS_STREAMING, I.w, I.h);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(texture, nullptr, I.data,
                      sizeof(unsigned char) * I.w * I.ch);
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = I.w;
    rect.h = I.h;
    SDL_RenderCopy(renderer, texture, nullptr, &rect);
    SDL_DestroyTexture(texture);
  }
}

void MyPainter::paint() {
  if (useOpenGLForDrawingInsteadOfSDL) {
    // not implemented
  } else {
    SDL_RenderPresent(renderer);
  }
}

void MyPainter::clearToColor() {
  if (useOpenGLForDrawingInsteadOfSDL) {
    glClearColor(currColor.r / 255.f, currColor.g / 255.f, currColor.b / 255.f,
                 currColor.a / 255.f);
    glClear(GL_COLOR_BUFFER_BIT);
  } else {
    SDL_SetRenderDrawColor(renderer, currColor.r, currColor.g, currColor.b,
                           currColor.a);
    SDL_RenderClear(renderer);
  }
}

int MyPainter::getCurrentThickness() { return currThickness; }
