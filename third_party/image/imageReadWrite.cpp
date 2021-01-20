// Copyright (c) 2017 Marek Dvoroznak
// Licensed under the MIT License.

#include "image.h"

#ifdef IMAGE_READ_WRITE
namespace ImageExternal {
#ifndef IMAGE_READ_WRITE_NO_COMPRESSION
#ifndef IMAGE_MINIZ_EXTERNAL
  #include "miniz.c"
#endif
#endif
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
};

template<>
Imguc Imguc::loadImage(const std::string &filename, int alphaChannel, int desiredNumChannels)
{
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::loadImage: Loading 8 bit per channel image (" << filename << ")" << std::endl;);
  int width, height, ch;
  unsigned char *data = ImageExternal::stbi_load(filename.c_str(), &width, &height, &ch, desiredNumChannels);
  if (desiredNumChannels > 0) ch = desiredNumChannels;
  if (data == nullptr) {
    DEBUG_CMD_IMG(std::cout << debugPrefix() << "::loadImage: Failed loading image " << std::endl;);
    return nullImage();
  }

  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::loadImage: w: " << width << ", h: " << height << ", ch: " << ch << ", alphaChannel: " << alphaChannel << std::endl;);
  Imguc img = Imguc(data, width, height, ch, alphaChannel);
  img.sharedData = false;
  return img;
}

template<>
Imguc Imguc::loadPNG(const std::string &filename, int alphaChannel, int desiredNumChannels)
{
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::loadImage: Loading 8 bit per channel image (" << filename << ")" << std::endl;);
  return loadImage(filename, alphaChannel, desiredNumChannels);
}

template<>
Imguc Imguc::loadImage(unsigned char *imgData, int length, int alphaChannel, int desiredNumChannels)
{
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::loadImage: Loading 8 bit per channel image" << std::endl;);
  int width, height, ch;
  unsigned char *data = ImageExternal::stbi_load_from_memory(imgData, length, &width, &height, &ch, desiredNumChannels);
  if (desiredNumChannels > 0) ch = desiredNumChannels;
  if (data == nullptr) {
    DEBUG_CMD_IMG(std::cout << debugPrefix() << "::loadImage: Failed loading image " << std::endl;);
    return nullImage();
  }

  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::loadImage: w: " << width << ", h: " << height << ", ch: " << ch << ", alphaChannel: " << alphaChannel << std::endl;);
  Imguc img = Imguc(data, width, height, ch, alphaChannel);
  img.sharedData = false;
  return img;
}

template<>
Imguc Imguc::loadPNG(unsigned char *imgData, int length, int alphaChannel, int desiredNumChannels)
{
  return loadImage(imgData, length, alphaChannel, desiredNumChannels);
}

template<>
Image16 Image16::loadPNG(const std::string &filename, int alphaChannel, int desiredNumChannels)
{
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::loadPNG: Loading 16 bit per channel PNG image (" << filename << ")" << std::endl;);
  int width, height, ch;
  unsigned short *data = ImageExternal::stbi_load_16(filename.c_str(), &width, &height, &ch, desiredNumChannels);
  if (desiredNumChannels > 0) ch = desiredNumChannels;
  if (data == nullptr) {
    DEBUG_CMD_IMG(std::cout << debugPrefix() << "::loadPNG: Failed loading image " << std::endl;);
    return nullImage();
  }
  return Image16(data, width, height, ch, alphaChannel);
}

template<>
bool Imguc::savePNG(const std::string &filename) const
{
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::savePNG: Saving 8 bit per channel PNG image (" << filename << ")" << std::endl;);
  int i = ImageExternal::stbi_write_png(filename.c_str(), w, h, ch, data, w*ch);
  if (i == 0) {
    DEBUG_CMD_IMG(std::cout << debugPrefix() << "::savePNG: Failed saving image " << std::endl;);
    return false;
  }
  return true;
}

template<>
bool Imguc::savePNG(unsigned char *&pngData, int &length) const
{
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::savePNG: Saving 8 bit per channel PNG image to memory" << std::endl;);
  pngData = ImageExternal::stbi_write_png_to_mem(data, w*ch, w, h, ch, &length);
  if (pngData == nullptr) {
    DEBUG_CMD_IMG(std::cout << debugPrefix() << "::savePNG: Failed saving image " << std::endl;);
    return false;
  }
  return true;
}
#endif
