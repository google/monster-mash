// Copyright (c) 2017 Marek Dvoroznak
// Licensed under the MIT License.

#include "image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>

template<>
Imguc Imguc::scale(int newW, int newH)
{
  if (newW == w && newH == h) return *this;

  Imguc O(newW, newH, ch, alphaChannel);
  stbir_resize_uint8(data, w, h, 0, O.data, newW, newH, 0, ch);
  return O;
}

template<>
Imguc Imguc::scaleWidth(int newW)
{
  const float r = h/static_cast<float>(w);
  const int newH = r * newW;
  return scale(newW, newH);
}

template<>
Imguc Imguc::scaleHeight(int newH)
{
  const float r = w/static_cast<float>(h);
  const int newW = r * newH;
  return scale(newW, newH);
}
