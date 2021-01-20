// Copyright (c) 2017 Marek Dvoroznak
// Licensed under the MIT License.

#ifndef IMAGEUTILS_H
#define IMAGEUTILS_H

#include "image.h"
#include <iostream>
#include <Eigen/Core>

void floodFill(Imguc &I, int startX, int startY, unsigned char targetColor, unsigned char replacementColor);
void floodFill(const Imguc &I, Imguc &O, int startX, int startY, unsigned char targetColor, unsigned char replacementColor);
void scanlineFillOutline(const Imguc &outline, Imguc &out, unsigned char outlineColor);

template<typename T>
bool traceRegion(const Imguc &M, T fcn)
{
  int startX;
  int startY;
  return traceRegion(M, fcn, startX, startY);
}

// trace region clockwise
// M is a mask for background; everything with 0 in the mask is treated as foreground
template<typename T>
bool traceRegion(const Imguc &M, T fcn, int &startX, int &startY)
{
  using namespace std;
  using namespace Eigen;

  // analogy to a robot walking along a wall in a maze
  // find a place to start
  startX=-1; startY=-1;
  fora(y,0,M.h) fora(x,0,M.w) { if (M(x,y,0) == 0) { startX=x; startY=y; goto EXIT; } } EXIT:
  if (startX==-1 || startY==-1) {
    // region not found, terminate
    return false;
  }

  DEBUG_CMD_IMG(cout << "start: " << startX << " " << startY << endl;)

  /*
    current direction: x+i, y+j
      i=1,j=0  >
      i=0,j=1  v
      i=-1,j=0 <
      i=0,j=-1 ^

    step ahead
    no wall on the left side, turn left
    wall ahead, turn right
  */

  // possible directions
  vector<Vector2i> dir = { {1,0},  // >
                           {0,1},  // v
                           {-1,0}, // <
                           {0,-1}  // ^
                         };
  int currentDir = 0;
  int x = startX, y = startY;
  int inPlaceCount = 0;
  do {
START:
    int l = (4+currentDir-1)%4;
    int r = (currentDir+1)%4;
    {
      Vector2i &ca = dir[currentDir];
      Vector2i &cl = dir[l];
      if (M.checkBounds(x+cl(0),y+cl(1),0) && M(x+cl(0),y+cl(1),0) == 0) currentDir = l; // no wall on the left side, turn left
      else if (!M.checkBounds(x+ca(0),y+ca(1),0) || M(x+ca(0),y+ca(1),0) != 0) { currentDir = r; inPlaceCount++; if (inPlaceCount > 3) break; goto START; } // wall ahead, turn right
    }
    inPlaceCount = 0;
    Vector2i &ca = dir[currentDir];

    fcn(x,y);

    x+=ca(0); y+=ca(1); // step ahead
  } while (!(x == startX && y == startY));
  return true;
}

// trace region clockwise
template<typename T>
bool traceRegion(const Imguc &I, const int segId, T fcn, int &startX, int &startY)
{
  using namespace std;
  using namespace Eigen;

  // find a place to start
  startX=-1; startY=-1;
  fora(y,0,I.h) fora(x,0,I.w) { if (I(x,y,0) == segId) { startX=x; startY=y; goto EXIT; } } EXIT:
  if (startX==-1 || startY==-1) {
    DEBUG_CMD_IMG(cout << "region not found" << endl;)
    return false;
  }

  // possible directions
  vector<Vector2i> dir = { {1,0},  // >
                           {0,1},  // v
                           {-1,0}, // <
                           {0,-1}  // ^
                         };
  int currentDir = 0;
  int x = startX, y = startY;
  int inPlaceCount = 0;
  do {
START:
    int l = (4+currentDir-1)%4;
    int r = (currentDir+1)%4;
    {
      Vector2i &ca = dir[currentDir];
      Vector2i &cl = dir[l];
      if (I(x+cl(0),y+cl(1),0) == segId) currentDir = l; // no wall on the left side, turn left
      else if (I(x+ca(0),y+ca(1),0) != segId) { currentDir = r; inPlaceCount++; if (inPlaceCount > 3) break; goto START; } // wall ahead, turn right
    }
    inPlaceCount = 0;
    Vector2i &ca = dir[currentDir];

    fcn(x,y);

    x+=ca(0); y+=ca(1); // step ahead
  } while (!(x == startX && y == startY));
  return true;
}

template<typename T>
void dilateSquare(const Img<T> &I, Img<T> &O, const Color<T> &backgroundColor, const int N = 1)
{
  if (!I.equalDimension(O)) O.initImage(I);
  O.fill(backgroundColor);
  fora(y, 0, I.h) fora(x, 0, I.w) {
    bool hit = false;
    Color<T> color;
    fora(j, -N, N+1) fora(i, -N, N+1) {
      if (I.checkBounds(x+i, y+j, 0)) {
        color = I.getPixel(x+i,y+j);
        if (color != backgroundColor) {
          hit = true;
          goto EXIT;
        }
      }
    }
    EXIT:
    if (hit) {
      fora(c, 0, I.ch) {
//        O(x,y,c) = I(x,y,c);
        O.getPixel(x,y) = color;
      }
    }
  }
}

template<typename T>
Img<T> dilateSquare(const Img<T> &I, const Color<T> &backgroundColor, const int N = 1)
{
  Img<T> O;
  dilateSquare(I, O, backgroundColor, N);
  return O;
}

template<typename T>
void erodeSquare(const Img<T> &I, Img<T> &O, const Color<T> &backgroundColor, const int N = 1)
{
  if (!I.equalDimension(O)) O.initImage(I);
  O.fill(backgroundColor);
  fora(y, 0, I.h) fora(x, 0, I.w) {
    bool miss = false;
    Color<T> color;
    fora(j, -N, N+1) fora(i, -N, N+1) {
      if (I.checkBounds(x+i, y+j, 0)) {
        color = I.getPixel(x+i,y+j);
        if (color == backgroundColor) {
          miss = true;
          goto EXIT;
        }
      }
    }
    EXIT:
    if (!miss) {
      fora(c, 0, I.ch) {
        O.getPixel(x,y) = color;
      }
    }
  }
}

template<typename T>
Img<T> erodeSquare(const Img<T> &I, const Color<T> &backgroundColor, const int N = 1)
{
  Img<T> O;
  erodeSquare(I, O, backgroundColor, N);
  return O;
}

template<typename T>
void openSquare(const Img<T> &I, Img<T> &O, const Color<T> &backgroundColor, const int N = 1)
{
  erodeSquare(I, O, backgroundColor, N);
  O = dilateSquare(O, backgroundColor, N);
}

template<typename T>
Img<T> openSquare(const Img<T> &I, const Color<T> &backgroundColor, const int N = 1)
{
  Img<T> O;
  openSquare(I, O, backgroundColor, N);
  return O;
}

template<typename T>
void closeSquare(const Img<T> &I, Img<T> &O, const Color<T> &backgroundColor, const int N = 1)
{
  dilateSquare(I, O, backgroundColor, N);
  O = erodeSquare(O, backgroundColor, N);
}

template<typename T>
Img<T> closeSquare(const Img<T> &I, const Color<T> &backgroundColor, const int N = 1)
{
  Img<T> O;
  closeSquare(I, O, backgroundColor, N);
  return O;
}

#endif // IMAGEUTILS_H
