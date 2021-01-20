// Copyright (c) 2017 Marek Dvoroznak
// Licensed under the MIT License.

namespace ImageExternal {
#ifndef IMAGE_READ_WRITE_NO_COMPRESSION
#ifdef IMAGE_MINIZ_EXTERNAL
  #include <miniz.h>
#else
  #include "miniz.h"
#endif
#endif
};

template<typename T>
bool Img<T>::saveRaw(const std::string &filename, bool includeHeader) const
{
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::saveRaw" << std::endl;)
  return saveToFile(filename, reinterpret_cast<const char*>(data), w*h*ch*getBytesPerChannel(), false, includeHeader);
}

#ifndef IMAGE_READ_WRITE_NO_COMPRESSION
template<typename T>
bool Img<T>::save(const std::string &filename, bool includeHeader) const
{
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::save: Saving compressed image" << std::endl;);
  ImageExternal::mz_ulong src_len = w*h*ch*getBytesPerChannel();
  ImageExternal::mz_ulong cmp_len = ImageExternal::mz_compressBound(src_len);
  ImageExternal::mz_uint8 *pCmp = (ImageExternal::mz_uint8*)malloc((size_t)cmp_len);
  ImageExternal::mz_uint8 *pUncomp = (ImageExternal::mz_uint8*)data;
  if (pCmp == NULL) {
    DEBUG_CMD_IMG(std::cout << debugPrefix() << "::save: Memory allocation error" << std::endl;);
    return false; // memory allocation error
  }
  int cmp_status = ImageExternal::mz_compress(pCmp, &cmp_len, (const ImageExternal::mz_uint8*)data, src_len);
  if (cmp_status != ImageExternal::MZ_OK) {
    DEBUG_CMD_IMG(std::cout << debugPrefix() << "::save: Compression failed" << std::endl;);
    return false; // compression failed
  }

  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::save: Original size: " << src_len << ", compressed size: " << cmp_len << std::endl;)

  bool saved = saveToFile(filename, reinterpret_cast<const char*>(pCmp), cmp_len, true, includeHeader);
  free(pCmp);
  return saved;
}
#endif

template<typename T>
Img<T> Img<T>::load(const std::string &filename, int width, int height, int channels, int alphaChannel, int bytesPerChannel, bool compressed)
{
  std::ifstream stream;
  stream.open(filename, std::ifstream::binary);
  if (!stream.is_open()) return Img::nullImage();
  assert(bytesPerChannel == sizeof(T));
  stream.seekg(0, stream.end);
  int length = stream.tellg();
  stream.seekg(0, stream.beg);

  return loadFileFromStream(stream, width, height, channels, alphaChannel, bytesPerChannel, compressed, length);
}

template<typename T>
Img<T> Img<T>::load(const std::string &filename)
{
  std::ifstream stream;
  stream.open(filename, std::ifstream::binary);
  if (!stream.is_open()) return Img::nullImage();
  int w, h, ch, alphaChannel, bytesPerChannel, length; bool compressed;
  stream >> w >> h >> ch >> alphaChannel >> bytesPerChannel >> compressed >> length;
  assert(bytesPerChannel == sizeof(T));
  stream.seekg(1, stream.cur);

  return loadFileFromStream(stream, w, h, ch, alphaChannel, bytesPerChannel, compressed, length);
}

//template<typename T>
//Img<T> Img<T>::loadPNG(const std::string &filename)
//{
//  assert(false && "loadPNG is implemented for 8 and 16 bit for channel PNGs only; did you include imageReadWrite.cpp file?");
//}

//template<typename T>
//bool Img<T>::savePNG(const std::string &filename) const
//{
//  assert(false && "savePNG is implemented for 8 bit for channel PNGs only; did you include imageReadWrite.cpp file?");
//}

template<typename T>
bool Img<T>::saveToFile(const std::string &filename, const char* data, int length, bool compressed, bool includeHeader) const
{
  std::ofstream stream;
  stream.open(filename, std::ifstream::binary);
  if (includeHeader) stream << w << " " << h << " " << ch << " " << alphaChannel << " " << getBytesPerChannel() << " " << compressed << " " << length << " ";
  stream.write(data, length);
  if (!stream) return false;
  stream.close();
  return true;
}

template<typename T>
Img<T> Img<T>::loadFileFromStream(std::ifstream &stream, int width, int height, int channels, int alphaChannel, int bytesPerChannel, bool compressed, int length)
{
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::load: Loading image" << std::endl;);
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::load: w: " << width << ", h: " << height << ", ch: " << channels << ", alphaChannel: " << alphaChannel << ", bytesPerChannel: " << bytesPerChannel << ", compressed: " << compressed << ", length: " << length << std::endl;);

  char *fileData = new char[length];
  stream.read(fileData, length);
  if (!stream) return Img::nullImage();

  char *data = NULL;

  if (compressed) {
#ifndef IMAGE_READ_WRITE_NO_COMPRESSION
    ImageExternal::mz_ulong cmp_len = length;
    ImageExternal::mz_ulong uncomp_len = width*height*channels*bytesPerChannel;
    ImageExternal::mz_uint8 *pUncomp = reinterpret_cast<ImageExternal::mz_uint8*>(new char[uncomp_len]);
    ImageExternal::mz_uint8 *pCmp = reinterpret_cast<ImageExternal::mz_uint8*>(fileData);
    int cmp_status = ImageExternal::mz_uncompress(pUncomp, &uncomp_len, pCmp, cmp_len);
    if (cmp_status != ImageExternal::MZ_OK) {
      DEBUG_CMD_IMG(std::cout << debugPrefix() << "::load: Failed to perform decompression" << std::endl;);
      return Img::nullImage(); // decompression failed
    }
    delete[] fileData;
    data = reinterpret_cast<char*>(pUncomp);
#else
    assert(!"Image: IMAGE_READ_WRITE_NO_COMPRESSION defined, compression is not available");
#endif
  } else {
    data = fileData;
  }

  Img img(reinterpret_cast<T*>(data), width, height, channels, alphaChannel);
  img.sharedData = false;
  return img;
}
