// Copyright (c) 2017 Marek Dvoroznak
// Licensed under the MIT License.

#include "imageUtils.h"
#include <stack>

using namespace std;
using namespace Eigen;

void floodFill(Imguc &I, int startX, int startY, unsigned char targetColor, unsigned char replacementColor)
{
  stack<pair<int,int>> S;
  S.push(make_pair(startX,startY));
  while (!S.empty()) {
    const pair<int,int> &front = S.top();
    const int x = front.first, y = front.second;
    S.pop();
    if (x < 0 || x >= I.w || y < 0 || y >= I.h) continue;
    unsigned char &sample = I(x, y, 0);
    if (targetColor == replacementColor || sample != targetColor) continue;
    sample = replacementColor;
    S.push(make_pair(x, y+1));
    S.push(make_pair(x, y-1));
    S.push(make_pair(x-1, y));
    S.push(make_pair(x+1, y));
  }
}

void floodFill(const Imguc &I, Imguc &O, int startX, int startY, unsigned char targetColor, unsigned char replacementColor)
{
  stack<pair<int,int>> S;
  S.push(make_pair(startX,startY));
  while (!S.empty()) {
    const pair<int,int> &front = S.top();
    const int x = front.first, y = front.second;
    S.pop();
    if (x < 0 || x >= I.w || y < 0 || y >= I.h) continue;
    const unsigned char &sampleI = I(x, y, 0);
    unsigned char &sampleO = O(x, y, 0);
    if (sampleI != targetColor || sampleO == replacementColor) continue;
//    sampleI == targetColor && sampleO != replacementColor
    sampleO = replacementColor;
    S.push(make_pair(x, y+1));
    S.push(make_pair(x, y-1));
    S.push(make_pair(x-1, y));
    S.push(make_pair(x+1, y));
  }
}

void scanlineFillOutline(const Imguc &outline, Imguc &out, unsigned char outlineColor)
{
  const Imguc &O = outline;
  fora(y,1,out.h-1) {
    bool inside = false;
    unsigned char prev = O(0,0,0);
    int stepUp = 0, stepDown = 0;
    fora(x,1,out.w-1) {
      const unsigned char sample = O(x,y,0);
      if (sample != prev) {
        if (sample == outlineColor) { stepUp = 0; stepDown = 0; }
        if (sample == outlineColor && (O(x-1,y+1,0)==outlineColor || O(x,y+1,0)==outlineColor)) stepUp++; // start outline, step up
        if (sample != outlineColor && (O(x-1,y-1,0)==outlineColor || O(x,y-1,0)==outlineColor)) stepUp++; // end outline, step up
        if (sample == outlineColor && (O(x-1,y-1,0)==outlineColor || O(x,y-1,0)==outlineColor)) stepDown++; // start outline, step down
        if (sample != outlineColor && (O(x-1,y+1,0)==outlineColor || O(x,y+1,0)==outlineColor)) stepDown++; // end outline, step down
        if (sample != outlineColor && !(stepUp==stepDown && stepUp == 1)) inside = !inside;
      }
      if (inside) out(x,y,0) = outlineColor;
      prev = sample;
    }
  }
}
