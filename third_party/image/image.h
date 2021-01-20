// Copyright (c) 2017 Marek Dvoroznak
// Licensed under the MIT License.

// Define IMAGE_READ_WRITE for writing and reading methods to be included.
// Define IMAGE_READ_WRITE_NO_COMPRESSION for not including miniz.
// Define IMAGE_SCALE for up-/down- scaling (using stb_image_resize)

#ifndef IMAGE_H
#define IMAGE_H

#include <iostream>
#include <stdexcept>
#include <cmath>
#include <cassert>
#include <vector>
#include <cstring>
#include <string>
#include <fstream>
#include <utility>
#include <map>
#include <algorithm>

#ifndef DEBUG_CMD_IMG
  #ifdef ENABLE_DEBUG_CMD_IMG
    #define DEBUG_CMD_IMG(x) {x}
  #else
    #define DEBUG_CMD_IMG(x) ;
  #endif
#endif
#ifndef fora
  #define fora(a,s,e) for (int a=s; a<e; a++)
#endif
#ifndef forlist
  #define forlist(a,s) for (size_t a=0; a<(s).size(); a++)
#endif

template<typename T>
class Color
{
public:
  int ch;
  T *data = nullptr;
  bool sharedData;

  Color();
  virtual ~Color();

//  Color(Color &&c) = default;
  Color(int ch);
  Color(int ch, const T &defaultValue);
  Color(const Color &c);
  Color(int ch, T *data);
  Color(std::initializer_list<T> init_list);
  template<typename U> Color<T>& initColor(const Color<U> &from, bool sharedData = false);
  inline const Color& operator=(const Color &c);
  inline const T& operator()(int ch) const;
  inline T& operator()(int ch);
  Color head(int n);
  template<typename U> Color<U> cast();
  bool isNull() const;

  inline Color& operator/=(const Color<T> &rhs);
  inline Color<T>& operator/=(const T &c);
  inline Color& operator+=(const Color<T> &rhs);
  inline Color<T>& operator+=(const T &c);
};


template<typename T>
class Img
{
public:
  T *data = nullptr;
  int w;
  int h;
  int ch; // number of channels
  int alphaChannel;
  bool sharedData;
  int valuesPerChannel = -1;

  enum BoundaryHandling { clip, reflect, none };
  enum Sampling { nearest, bilinear };

  // If the input image has shared data, the result is an image also with shared data.
  // The same holds for new data.
  Img(const Img &img);
  Img();
  Img(int w, int h, int channels, int alphaChannel = -1);
  Img(T *data, int w, int h, int channels);
  Img(T *data, int w, int h, int channels, int alphaChannel);
  Img(const Img &img, int nChannelsFactor, int alphaChannel = -1);

  virtual ~Img();

  static const Img nullImage();
  bool isNull() const;
  Img<T>& setNull();

  Img& clear();
  Img& fill(const Color<T> &c);
  Img& fill(const T &value);

  // Returns a deep copy of this image (of type T) which has its data casted to type U.
  template<typename U> Img<U> cast() const;

  Img<T> cast() const;

  // The data of the image on the left will be rewritten by the data of the image on the right (deep copy)
  // and type conversion will be performed.
  // If the image on the left is NULL image, then it is initialized according to the image on the right.
  template<typename U> Img& cast(const Img<U> &img);

  Img& cast(const Img<T> &img);

  // Returns a deep copy of this image (of type T) which has its data casted to type U
  // and also all values scaled according to the new type.
  template<typename U> Img<U> castAndConvert() const;

  Img<T> castAndConvert() const;

  // The data of the image on the left will be rewritten by the data of the image on the right (deep copy)
  // and type conversion will be performed and all values scaled according to the type T.
  // If the image on the left is NULL image, then it is initialized according to the image on the right.
  template<typename U> Img& castAndConvert(const Img<U> &img);

  Img& castAndConvert(const Img<T> &img);

  // If the type of the image on the left differs from the type of the image on the right,
  // deep copy with conversion (castAndConvert() method) is performed.
  // Otherwise it returns a shallow copy.
  template<typename U> Img<U> shallowCopyCastAndConvert() const;

  // If the type of the image on the left differs from the type of the image on the right,
  // deep copy with conversion (castAndConvert() method) is performed.
  // Otherwise it returns a shallow copy.
  Img<T> shallowCopyCastAndConvert() const;

  template<typename U> Img& shallowCopyCastAndConvert(const Img<U> &img);

  Img& shallowCopyCastAndConvert(const Img<T> &img);

  // No matter if the image has or has not shared data, the result is an image with new data.
  Img deepCopy() const;

  // No matter if the image on the left has or has not shared data, the same for the image on the right,
  // the data of the image on the left will be rewritten by the data of the image on the right (deep copy).
  // If the image on the left is NULL image, then it is initialized according to the image on the right.
  Img& deepCopy(const Img &img);

  // No matter if the image has or has not shared data, the result is an image with shared data.
  Img shallowCopy() const;

  const Img<const T>& constShallowCopy() const;

  // No matter if the image on the left has or has not shared data, the same for the image on the right,
  // the image on the left will become image with shared data, i.e. the pointer to data of the image on the left
  // will point to the data of the image on the right (shallow copy).
  // If the image on the left is NULL image, then it is initialized according to the image on the right.
  // If the image on the left has not shared data then the data will be freed.
  Img& shallowCopy(const Img &img);

  // If the image on the left has shared data, it will still have shared data after the assignment.
  // The same holds for new data.
  const Img& operator=(const Img &img);

//  Img& operator=(Img &&img);

  // check out of range
  const T& at(int x, int y, int channel) const;

  // check out of range
  T& at(int x, int y, int channel);

  // check out of range
  T sampleBilinearAt(double x, double y, int channel) const;

  // do not check out of range
  T sampleBilinear(double x, double y, int channel) const;
  T sampleBilinear(double x, double y, int channel, BoundaryHandling bndHandling) const;

  // do not check out of range
  T sampleNearest(double x, double y, int channel) const;
  T sampleNearest(double x, double y, int channel, BoundaryHandling bndHandling) const;

  // do not check out of range
  T sample(double x, double y, int channel, BoundaryHandling bndHandling, Sampling sampling) const;

  // do not check out of range
  Color<T> samplePixel(double x, double y, BoundaryHandling bndHandling, Sampling sampling) const;

  // Access image data by x, y coordinates and channel.
  // do not check out of range
  inline const T& operator()(int x, int y, int channel) const;

  // Access image data by x, y coordinates and channel.
  // do not check out of range
  inline T& operator()(int x, int y, int channel);

  inline const T& operator()(int x, int y, int channel, BoundaryHandling bndHandling) const;
  inline T& operator()(int x, int y, int channel, BoundaryHandling bndHandling);

  // Access image data by index and channel.
  // do not check out of range
  inline const T& operator()(int index, int channel) const;

  // Access image data by index and channel.
  // do not check out of range
  inline T& operator()(int index, int channel);

  // Access alpha channel by x, y coordinates.
  // do not check out of range
  inline const T& alpha(int x, int y) const;

  // Access alpha channel by x, y coordinates.
  // do not check out of range
  inline T& alpha(int x, int y);

  inline Img& operator+=(const T &t);
  template<typename U> inline Img& operator-=(const U &t);
  inline Img& operator*=(const T &t);
  inline Img& operator/=(const T &t);

  inline Img& operator+=(const Img &rhs);
  inline Img& operator-=(const Img &rhs);
  inline Img& operator*=(const Img &rhs);
  inline Img& operator/=(const Img &rhs);

//  inline const Color<T>& getPixel(int x, int y) const;
//  inline Color<T>& getPixel(int x, int y);
  inline const Color<T> getPixel(int x, int y) const;
  inline Color<T> getPixel(int x, int y);

  static int getBytesPerChannel();
  static int getMaxValuesPerChannel();
  int getValuesPerChannel() const;
  void setValuesPerChannel(int val);
  int getTotalBytes();
  int wh() const;
  int whch() const;

  template<typename U> Img& initImage(const Img<U> &fromImg, bool sharedData = false);

  // convert to premultiplied alpha
  Img& multiplyByAlpha();
  // convert to postmultiplied alpha
  Img& divideByAlpha();
  Img getChannel(int channel, int alphaChannel = -1) const;
  Img getChannels(std::vector<int> channels, int alphaChannel = -1) const;
  Img cloneChannels(int nChannelsFactor, int alphaChannel = -1) const;
  Img& replaceChannels(const Img &inImg, std::vector<int> inImgChannels, std::vector<int> thisImgChannels);
  Img& swapChannels(int ch1, int ch2);
  Img getAlphaChannel() const;
  Img& replaceAlpha(const Img &alpha);
  void getBoundingBox(int &x1, int &y1, int &x2, int &y2, int channel, int minVal, int maxVal) const;
  Img crop(int x1, int y1, int x2, int y2, int &modX1, int &modY1, int &modX2, int &modY2) const;
  Img crop(int w, int h, int shiftX = 0, int shiftY = 0) const;
  Img resize(int w, int h, int shiftX = 0, int shiftY = 0) const;
  Img resize(int w, int h, int shiftX, int shiftY, const Color<T> &c) const;
  Img& blendWithInPlace(const Img &topImg);
  // Convolution of the image with 1D kernel. Boundary reflection or clipping (default).
  Img<float> conv1D(const std::vector<float> &kernel, BoundaryHandling bndHandling = clip);
  Img& blur(float sigma);
  Img& invert(T maxVal);
  Img transpose() const;
  Img& clamp(T min, T max);
  bool equalData(const Img &img) const;
  bool equalDimension(const Img &img) const;
  void computeCentroid(double &cx, double &cy, T threshold = 0);
  Img warp(const Img<float> &Dx, const Img<float> &Dy, BoundaryHandling bndHandling = clip, Sampling sampling = bilinear);
  static Img warp(const Img &img, const Img<float> &Dx, const Img<float> &Dy, BoundaryHandling bndHandling = clip, Sampling sampling = bilinear);
  void reduceColors(const std::vector<Color<T>> &colors, const std::vector<int> &count, double threshold);
  void reduceColors(const std::vector<Color<T>> &colors, const std::vector<int> &count, double threshold, int &maxColors);
  static void reduceColors(Img &img, const std::vector<Color<T>> &colors, const std::vector<int> &count, double threshold);
  static void reduceColors(Img &img, const std::vector<Color<T>> &colors, const std::vector<int> &count, double threshold, int &maxColors);
  void reduceColors(const std::vector<Color<T>> &colors, const std::vector<int> &count, int maxColors);
  static void reduceColors(Img &img, const std::vector<Color<T>> &colors, const std::vector<int> &count, int maxColors);
  void getColorsUsage(std::vector<Color<T>> &colors, std::vector<int> &count);
  static void getColorsUsage(const Img &img, std::vector<Color<T>> &colors, std::vector<int> &count);
  void getColorsUsage(std::vector<Color<T>> &colors, std::vector<int> &count, const Img &alphaChannel);
  static void getColorsUsage(const Img &img, std::vector<Color<T>> &colors, std::vector<int> &count, const Img &alphaChannel);
  static void replaceColors(Img &img, const std::map<const Color<T>,Color<T>> &replacement);
  void replaceColors(const std::map<const Color<T>,Color<T>> &replacement);
  void minMax(std::vector<T> &min, std::vector<T> &max) const;
  Img subsample(int subsampleFactor, Sampling sampling) const;
  bool checkBounds(int x, int y, int c = 0) const;


private:
  void updateColorData();
  static std::string debugPrefix();
  static T bilinearInterpolation(T I0, T I1, T I2, T I3, double dx, double dy);
  static double alphaBlending(double A, double B, double BAlpha);
  static inline float gauss(float x, float sigma);
  static std::vector<float> genGaussKernel(float sigma);


#ifdef IMAGE_READ_WRITE
public:
  bool saveRaw(const std::string &filename, bool includeHeader = true) const;
#ifndef IMAGE_READ_WRITE_NO_COMPRESSION 
  bool save(const std::string &filename, bool includeHeader = true) const;
#endif
  // Loading of raw data (uncompressed or compressed) without a header.
  static Img load(const std::string &filename, int width, int height, int channels, int alphaChannel, int bytesPerChannel, bool compressed);
  // Loading of data with a header.
  static Img load(const std::string &filename);
  // Load from 8 or 16 bit per channel PNG file.
  static Img loadPNG(const std::string &filename, int alphaChannel = -1, int desiredNumChannels = 0);
  // Load from 8 bit per channel PNG file from memory.
  static Img loadPNG(unsigned char *imgData, int length, int alphaChannel = -1, int desiredNumChannels = 0);
  // Load image from file. Should work for all image formats supported by stb_image.
  static Img loadImage(const std::string &filename, int alphaChannel = -1, int desiredNumChannels = 0);
  // Load image from memory. Should work for all image formats supported by stb_image.
  static Img loadImage(unsigned char *imgData, int length, int alphaChannel = -1, int desiredNumChannels = 0);
  
  // Save to 8 bit per channel PNG file.
  bool savePNG(const std::string &filename) const;
  // Save to 8 bit per channel PNG image in memory.
  bool savePNG(unsigned char *&pngData, int &length) const;
private:
  bool saveToFile(const std::string &filename, const char* data, int length, bool compressed, bool includeHeader = true) const;
  static Img loadFileFromStream(std::ifstream &stream, int width, int height, int channels, int alphaChannel, int bytesPerChannel, bool compressed, int length);
#endif

#ifdef IMAGE_SCALE
public:
  Img scale(int newW, int newH);
  // scale width and keep aspect ratio
  Img scaleWidth(int newW);
  // scale height and keep aspect ratio
  Img scaleHeight(int newH);
#endif
};

// typedefs
typedef Color<unsigned char> Cu;
typedef Color<unsigned char> Cuc;
typedef Color<float> Cf;
typedef Color<double> Cd;
typedef Img<unsigned char> Imguc;
typedef Img<float> Imgf;
typedef Img<double> Imgd;
typedef Img<unsigned char> Image8;
typedef Img<const unsigned char> Imagec8;
typedef Img<unsigned short> Image16;
typedef Img<const unsigned short> Imagec16;

#ifdef IMAGE_READ_WRITE
#include "imageReadWrite.hpp"
#endif
















// Implementation

// -----------
// Color
// -----------

template<typename T>
Color<T>::Color() : ch(0), sharedData(false) {
}

template<typename T>
Color<T>::~Color()
{
  if (!sharedData && data != nullptr) {
    delete[] data;
    data = nullptr;
  }
}

//template<typename T>
//Color<T>::Color(Color &&c)
//{
//    std::cout << "move";
//    ch = c.ch;
//    data = std::move(c.data);
//    sharedData = c.sharedData;
//}

template<typename T>
Color<T>::Color(int ch) : ch(ch), sharedData(false)
{
  data = new T[ch];
  std::memset(data, 0, ch*sizeof(T));
}

template<typename T>
Color<T>::Color(int ch, const T &defaultValue) : ch(ch), sharedData(false)
{
  data = new T[ch];
  fora(i, 0, ch) data[i] = defaultValue;
}

// all copies of this color are deep
template<typename T>
Color<T>::Color(const Color &c)
{
  initColor(c, false);
  *this = c;
}

template<typename T>
Color<T>::Color(int ch, T *data) : ch(ch), data(data), sharedData(true) {}

template<typename T>
Color<T>::Color(std::initializer_list<T> init_list) : sharedData(false)
{
  ch = init_list.size();
  data = new T[ch];
  int j = 0;
  for(const T &i : init_list) data[j++] = i;
}

template<typename T>
template<typename U>
Color<T>& Color<T>::initColor(const Color<U> &from, bool sharedData)
{
  ch = from.ch;
  this->sharedData = sharedData;
  if (!sharedData) data = new T[ch];
  else data = nullptr;
  return *this;
}

// all copies of this color are deep
template<typename T>
inline const Color<T>& Color<T>::operator=(const Color &c)
{
  if (ch == 0) initColor(c, false);
  assert(ch == c.ch);
  fora(i, 0, ch) data[i] = c.data[i];
  return *this;
}

template<typename T>
inline const T& Color<T>::operator()(int ch) const
{
  return data[ch];
}

template<typename T>
inline T& Color<T>::operator()(int ch)
{
  return const_cast<T&>(static_cast<const Color&>(*this)(ch));
}

template<typename T>
Color<T> Color<T>::head(int n)
{
  return Color(n, data);
}

template<typename T>
template<typename U>
Color<U> Color<T>::cast()
{
  Color<U> c;
  c.initColor(*this, false);
  fora(i,0,ch) c(i) = (U)operator()(i);
  return c;
}

template<typename T>
bool Color<T>::isNull() const
{
  return ch == 0;
}


//template<typename T>
//template<typename T2,int CH2>
//Color<T2,CH2> Color<T,CH>::cast()
//{
//  Color<T2,CH2> out;
//  fora(i, 0, std::min(CH,CH2)) out(i) = operator()(i);
//  return out;
//}

//template<typename T,int CH>
//template<typename COLOR>
//COLOR Color<T,CH>::cast()
//{
//  COLOR out;
//  fora(i, 0, std::min(CH,out.ch)) out(i) = operator()(i);
//  return out;
//}

template<typename T>
inline bool operator==(const Color<T> &l, const Color<T> &r)
{
  assert(l.ch == r.ch);
//  if (l.ch != r.ch) return false;
  fora(i, 0, l.ch) if (l(i) != r(i)) return false;
  return true;
}

template<typename T>
inline bool operator!=(const Color<T> &l, const Color<T> &r)
{
  return !(l==r);
}

template<typename T>
bool operator<(const Color<T> &l,const Color<T> &r)
{
  assert(l.ch == r.ch);
  double sum_l = 0, sum_r = 0;
  fora(i, 0, l.ch) {
    sum_l += pow(255,i) * l(i)*l(i);
    sum_r += pow(255,i) * r(i)*r(i);
  }
  return sum_l < sum_r;
}

template<typename T>
inline Color<T>& Color<T>::operator/=(const Color<T> &rhs)
{
  assert(ch == rhs.ch);
  fora(i, 0, ch) data[i] /= rhs.data[i];
  return *this;
}

template<typename T>
inline Color<T> operator/(const Color<T> &lhs, const Color<T> &rhs)
{
  Color<T> out = lhs;
  out /= rhs;
  return out;
}

template<typename T>
inline Color<T>& Color<T>::operator/=(const T &c)
{
  fora(i, 0, ch) data[i] /= c;
  return *this;
}

template<typename T>
inline Color<T> operator/(const Color<T> &lhs, const T &c)
{
  Color<T> out = lhs;
  out /= c;
  return out;
}

template<typename T>
inline Color<T>& Color<T>::operator+=(const T &c)
{
  fora(i, 0, ch) data[i] += c;
  return *this;
}

template<typename T>
inline Color<T>& Color<T>::operator+=(const Color<T> &rhs)
{
  assert(ch == rhs.ch);
  fora(i, 0, ch) data[i] += rhs.data[i];
  return *this;
}

template<typename T>
std::ostream& operator<<(std::ostream &os, const Color<T> &c)
{
//  os << std::string("Color<T=") + std::to_string(sizeof(T)) + ">" << ": w: " << Img3.w << ", h: " << Img3.h << ", ch: " << Img3.ch << ", alphaChannel: " << Img3.alphaChannel << ", sharedData: " << Img3.sharedData;
  fora(i, 0, c.ch) os << std::to_string(c(i)) << " ";
//  fora(i, 0, c.ch) os << c(i) << " ";
  return os;
}

// -----------
// Img
// -----------
template<typename T>
Img<T>::Img(const Img &img)
{
  w = img.w; h = img.h; ch = img.ch; alphaChannel = img.alphaChannel; sharedData = img.sharedData;
  if (!sharedData) data = new T[ch*w*h];
//  dataC = nullptr;
//  updateColorData();
  *this = img;
}

template<typename T>
Img<T>::Img() : data(nullptr), /*dataC(nullptr),*/ w(0), h(0), ch(0), alphaChannel(0), sharedData(false)
{}

template<typename T>
Img<T>::Img(int w, int h, int channels, int alphaChannel) : w(w), h(h), ch(channels), alphaChannel(alphaChannel), sharedData(false)
{
  data = new T[ch*w*h]; clear();
//  dataC = nullptr;
//  updateColorData();
}

template<typename T>
Img<T>::Img(T *data, int w, int h, int channels) : data(data), w(w), h(h), ch(channels), alphaChannel(-1), sharedData(true)
{
  assert(data != nullptr);
//  dataC = nullptr;
//  updateColorData();
}

template<typename T>
Img<T>::Img(T *data, int w, int h, int channels, int alphaChannel) : data(data), w(w), h(h), ch(channels), alphaChannel(alphaChannel), sharedData(true)
{
  assert(data != nullptr);
//  dataC = nullptr;
//  updateColorData();
}

template<typename T>
Img<T>::Img(const Img &img, int nChannelsFactor, int alphaChannel) : w(img.w), h(img.h), ch(nChannelsFactor*img.ch), alphaChannel(alphaChannel), sharedData(sharedData)
{
  data = new T[ch*w*h];
  fora(y,0,h) fora(x,0,w) fora(c,0,img.ch) fora(c2,0,nChannelsFactor) {
    operator()(x,y,c2*img.ch+c) = img(x,y,c);
  }
}

template<typename T>
Img<T>::~Img()
{
  if (!sharedData && data != nullptr) {
    DEBUG_CMD_IMG(std::cout << debugPrefix() << ": Deleting " << std::to_string(getTotalBytes()) << " bytes" << std::endl;);
    delete[] data;
    data = nullptr;
  }
//  if (dataC != nullptr) delete[] dataC;
}

template<typename T>
bool Img<T>::isNull() const
{
  return w == 0 && h == 0 && ch == 0;
}

template<typename T>
Img<T>& Img<T>::setNull()
{
  if (!sharedData && data != nullptr) {
    delete[] data;
    data = nullptr;
  }
  w = 0; h = 0; ch = 0; alphaChannel = -1; sharedData = false;
  return *this;
}

template<typename T>
const Img<T> Img<T>::nullImage() {
  return Img();
}

template<typename T>
Img<T>& Img<T>::clear()
{
  std::memset(data, 0, ch*w*h*getBytesPerChannel());
  return *this;
}

template<typename T>
Img<T>& Img<T>::fill(const Color<T> &color)
{
  assert(ch == color.ch);
  fora(y,0,h) fora(x,0,w) fora(c,0,ch) {
    operator()(x,y,c) = color(c);
  }
  return *this;
}

template<typename T>
Img<T>& Img<T>::fill(const T &value)
{
  int i = 0;
  while (i<w*h*ch) data[i++] = value;
  return *this;
}

template<typename T>
template<typename U>
Img<U> Img<T>::cast() const
{
  Img<U> img(w, h, ch, alphaChannel);
  fora(y, 0, h) fora(x, 0, w) fora(c, 0, ch) img(x,y,c) = (U)operator()(x,y,c);
  return img;
}

template<typename T>
Img<T> Img<T>::cast() const
{
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::cast: Conversion between the same types, doing just deep copy" << std::endl;);
  return deepCopy();
}

template<typename T>
template<typename U>
Img<T>& Img<T>::cast(const Img<U> &img)
{
  if (isNull()) initImage(img, false);
  assert(img.w == w && img.h == h && img.ch == ch);
  fora(y, 0, h) fora(x, 0, w) fora(c, 0, ch) operator()(x,y,c) = (T)img(x,y,c);
  return *this;
}

template<typename T>
Img<T>& Img<T>::cast(const Img<T> &img)
{
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::cast: Conversion between the same types, doing just deep copy" << std::endl;);
  deepCopy(img);
  return *this;
}

template<typename T>
template<typename U>
Img<U> Img<T>::castAndConvert() const
{
  Img<U> img(w, h, ch, alphaChannel);
  fora(y, 0, h) fora(x, 0, w) fora(c, 0, ch) img(x,y,c) = (U)(operator()(x,y,c)*(Img<U>::getValuesPerChannel()/(double)getValuesPerChannel()));
  return img;
}

template<typename T>
Img<T> Img<T>::castAndConvert() const
{
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::castAndConvert: Conversion between the same types, doing just deep copy" << std::endl;);
  return deepCopy();
}

template<typename T>
template<typename U>
Img<T>& Img<T>::castAndConvert(const Img<U> &img) {
  if (isNull()) initImage(img, false);
  assert(img.w == w && img.h == h && img.ch == ch);
  fora(y, 0, h) fora(x, 0, w) fora(c, 0, ch) operator()(x,y,c) = (T)(img(x,y,c)*(getValuesPerChannel()/(double)Img<U>::getValuesPerChannel()));
  return *this;
}

template<typename T>
Img<T>& Img<T>::castAndConvert(const Img<T> &img)
{
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::castAndConvert: Conversion between the same types, doing just deep copy" << std::endl;);
  deepCopy(img);
  return *this;
}

template<typename T>
template<typename U>
Img<U> Img<T>::shallowCopyCastAndConvert() const
{
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::shallowCopyCastAndConvert: conversion to type U=" << std::to_string(sizeof(U)) << std::endl;);
  return castAndConvert<U>();
}

template<typename T>
Img<T> Img<T>::shallowCopyCastAndConvert() const
{
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::shallowCopyCastAndConvert: Conversion between the same types, doing just shallow copy" << std::endl;);
  return shallowCopy();
}

template<typename T>
template<typename U>
Img<T>& Img<T>::shallowCopyCastAndConvert(const Img<U> &img)
{
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::shallowCopyCastAndConvert: conversion from type U=" << std::to_string(sizeof(U)) << std::endl;);
  castAndConvert<U>(img);
  return *this;
}

template<typename T>
Img<T>& Img<T>::shallowCopyCastAndConvert(const Img<T> &img)
{
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::shallowCopyCastAndConvert: Conversion between the same types, doing just shallow copy" << std::endl;);
  shallowCopy(img);
  return *this;
}

template<typename T>
Img<T> Img<T>::deepCopy() const
{
  Img img(w, h, ch, alphaChannel);
  img.deepCopy(*this);
  return img;
}

template<typename T>
Img<T>& Img<T>::deepCopy(const Img &img)
{
  if (isNull()) initImage(img, false);
  assert(img.w == w && img.h == h && img.ch == ch);
  memcpy(data, img.data, ch*w*h*getBytesPerChannel());
  return *this;
}

template<typename T>
Img<T> Img<T>::shallowCopy() const
{
  return Img(data, w, h, ch, alphaChannel);
}

template<typename T>
const Img<const T>& Img<T>::constShallowCopy() const
{
  return Img<const T>((const T*)data, w, h, ch, alphaChannel);
}

template<typename T>
Img<T>& Img<T>::shallowCopy(const Img &img)
{
  if (isNull()) initImage(img, true);
  assert(img.w == w && img.h == h && img.ch == ch);
  if (!sharedData && data != nullptr) {
    DEBUG_CMD_IMG(std::cout << debugPrefix() << ": Deleting " << std::to_string(getTotalBytes()) << " bytes" << std::endl;);
    delete[] data;
  }
  data = img.data;
  sharedData = true;
  return *this;
}

template<typename T>
const Img<T>& Img<T>::operator=(const Img &img)
{
  if (isNull()) initImage(img, img.sharedData);
  if (sharedData) {
    DEBUG_CMD_IMG(std::cout << debugPrefix() << "::operator=: shared data: shallow copy" << std::endl;);
    shallowCopy(img);
  } else {
    DEBUG_CMD_IMG(std::cout << debugPrefix() << "::operator=: data not shared: deep copy" << std::endl;);
    deepCopy(img);
  }
  return *this;
}

//template<typename T>
//Img<T>& Img<T>::operator=(Img &&img)
//{
//  if(this != &other) { // no self assignment

//    delete[] mArray;                               // delete this storage
//    mArray = std::exchange(other.mArray, nullptr); // leave moved-from in valid state
//    size = std::exchange(other.size, 0);
//  }
//  return *this;
//}

template<typename T>
const T& Img<T>::at(int x, int y, int channel) const
{
  assert(channel >= 0 && channel < ch);
  if (!(x >= 0 && x < w && y >= 0 && y < h)) throw(std::range_error(std::string("x:"+std::to_string(x)+" w:"+std::to_string(w))+" y:"+std::to_string(y)+" h:"+std::to_string(h)));
  return data[ch*(y*w+x)+channel];
}

template<typename T>
T& Img<T>::at(int x, int y, int channel)
{
  return const_cast<T&>(static_cast<const Img<T>&>(*this).at(x,y,channel));
}

template<typename T>
T Img<T>::sampleBilinearAt(double x, double y, int channel) const
{
  int fx = floor(x), fy = floor(y);
  const T& I0 = at(fx,fy,channel);
  const T& I1 = at(fx+1,fy,channel);
  const T& I2 = at(fx,fy+1,channel);
  const T& I3 = at(fx+1,fy+1,channel);
  return bilinearInterpolation(I0,I1,I2,I3,x-fx,y-fy);
}

template<typename T>
T Img<T>::sampleBilinear(double x, double y, int channel) const
{
  int fx = floor(x), fy = floor(y);
  const T& I0 = operator()(fx,fy,channel);
  const T& I1 = operator()(fx+1,fy,channel);
  const T& I2 = operator()(fx,fy+1,channel);
  const T& I3 = operator()(fx+1,fy+1,channel);
  return bilinearInterpolation(I0,I1,I2,I3,x-fx,y-fy);
}

template<typename T>
T Img<T>::sampleBilinear(double x, double y, int channel, BoundaryHandling bndHandling) const
{
  int fx = floor(x), fy = floor(y);
  const T& I0 = operator()(fx,   fy,   channel, bndHandling);
  const T& I1 = operator()(fx+1, fy,   channel, bndHandling);
  const T& I2 = operator()(fx,   fy+1, channel, bndHandling);
  const T& I3 = operator()(fx+1, fy+1, channel, bndHandling);
  return bilinearInterpolation(I0,I1,I2,I3,x-fx,y-fy);
}

template<typename T>
T Img<T>::sampleNearest(double x, double y, int channel) const
{
  return operator()(round(x), round(y), channel);
}

template<typename T>
T Img<T>::sampleNearest(double x, double y, int channel, BoundaryHandling bndHandling) const
{
  return operator()(round(x), round(y), channel, bndHandling);
}

template<typename T>
T Img<T>::sample(double x, double y, int channel, BoundaryHandling bndHandling, Sampling sampling) const
{
  switch (sampling) {
    case bilinear: return sampleBilinear(x, y, channel, bndHandling); break;
    case nearest: return sampleNearest(x, y, channel, bndHandling); break;
    default: return operator()(x, y, channel, bndHandling); break;
  }
}

template<typename T>
Color<T> Img<T>::samplePixel(double x, double y, BoundaryHandling bndHandling, Sampling sampling) const
{
  Color<T> color(ch);
  switch (sampling) {
    case bilinear: fora(i, 0, ch) color(i) = sampleBilinear(x, y, i, bndHandling); break;
    case nearest: fora(i, 0, ch) color(i) = sampleNearest(x, y, i, bndHandling); break;
    default: fora(i, 0, ch) color(i) = operator()(x, y, i, bndHandling); break;
  }
  return color;
}

template<typename T>
inline const T& Img<T>::operator()(int x, int y, int channel) const
{
  return data[ch*(y*w+x)+channel];
}

template<typename T>
inline T& Img<T>::operator()(int x, int y, int channel)
{
  return const_cast<T&>(static_cast<const Img<T>&>(*this)(x,y,channel));
}

template<typename T>
inline const T& Img<T>::operator()(int x, int y, int channel, BoundaryHandling bndHandling) const
{
  switch (bndHandling) {
    case reflect:
                                         /* w-1-(x-(w-1)) */                 /* h-1-(y-(h-1)) */
      return operator()(x<0 ? -x : (x>w-1 ? 2*(w-1)-x : x), y<0 ? -y : (y>h-1 ? 2*(h-1)-y : y), channel);
      break;
    case clip:
    default:
      return operator()(x<0 ? 0 : (x>w-1 ? w-1 : x), y<0 ? 0 : (y>h-1 ? h-1 : y), channel);
      break;
    case none:
      return operator()(x, y, channel);
      break;
  }
}

template<typename T>
inline T& Img<T>::operator()(int x, int y, int channel, BoundaryHandling bndHandling)
{
  return const_cast<T&>(static_cast<const Img<T>&>(*this)(x,y,channel,bndHandling));
}

template<typename T>
inline const T& Img<T>::operator()(int index, int channel) const
{
  return data[ch*index+channel];
}

template<typename T>
inline T& Img<T>::operator()(int index, int channel)
{
  return const_cast<T&>(static_cast<const Img<T>&>(*this)(index,channel));
}

template<typename T>
inline const T& Img<T>::alpha(int x, int y) const
{
  assert(alphaChannel != -1);
  return data[ch*(y*w+x)+alphaChannel];
}

template<typename T>
inline T& Img<T>::alpha(int x, int y)
{
  return const_cast<T&>(static_cast<const Img<T>&>(*this)(x,y,alphaChannel));
}

//template<typename T>
//inline const Color<T>& Img<T>::getPixel(int x, int y) const
//{
//  return dataC[y*w+x];
//}

//template<typename T>
//inline Color<T>& Img<T>::getPixel(int x, int y)
//{
//  return const_cast<Color<T>&>(static_cast<const Img<T>&>(*this).getPixel(x,y));
//}

template<typename T>
inline const Color<T> Img<T>::getPixel(int x, int y) const
{
  return Color<T>(ch, &data[ch*(y*w+x)]);
}

// The result of this method is a shared color because move constructor is used.
template<typename T>
inline Color<T> Img<T>::getPixel(int x, int y)
{
  return Color<T>(ch, &operator()(x,y,0));
}


//template<typename T>
//template<typename COLOR>
//inline const COLOR& Img<T>::getPixel(int x, int y) const
//{
//  return (reinterpret_cast<COLOR*>(data))[y*w+x];
//}

//template<typename T>
//template<typename COLOR>
//inline COLOR& Img<T>::getPixel(int x, int y)
//{
//  return const_cast<COLOR&>(static_cast<const Img<T>&>(*this).getPixel<COLOR>(x,y));
//}

template<typename T>
int Img<T>::getBytesPerChannel()
{
  return sizeof(T);
}

template<typename T>
int Img<T>::getMaxValuesPerChannel()
{
  return pow(2,sizeof(T)*8);
}

template<typename T>
int Img<T>::getValuesPerChannel() const
{
  return valuesPerChannel > 0 ? valuesPerChannel : getMaxValuesPerChannel();
}

template<typename T>
void Img<T>::setValuesPerChannel(int val)
{
  valuesPerChannel = val;
}

template<typename T>
int Img<T>::getTotalBytes()
{
  return ch*w*h*getBytesPerChannel();
}

template<typename T>
int Img<T>::wh() const
{
  return w*h;
}

template<typename T>
int Img<T>::whch() const
{
  return w*h*ch;
}

template<typename T>
template<typename U>
Img<T>& Img<T>::initImage(const Img<U> &fromImg, bool sharedData)
{
  w = fromImg.w;
  h = fromImg.h;
  ch = fromImg.ch;
  alphaChannel = fromImg.alphaChannel;
  this->sharedData = sharedData;
  if (!sharedData) {
    if (data != nullptr) delete[] data;
    data = new T[ch*w*h];
    clear();
  }
  updateColorData();
  DEBUG_CMD_IMG(std::cout << debugPrefix() << "::initImage: w: " << w << ", h: " << h << ", ch: " << ch << ", alphaChannel: " << alphaChannel << ", sharedData: " << sharedData << std::endl;);
  return *this;
}

template<typename T>
Img<T>& Img<T>::multiplyByAlpha()
{
  fora(y, 0, h) fora(x, 0, w) {
    double alpha = operator()(x,y,alphaChannel)/(double)(getValuesPerChannel()-1);
    fora(c, 0, ch) {
      if (c == alphaChannel) continue;
      operator()(x,y,c) = operator()(x,y,c)*alpha;
    }
  }
  return *this;
}

template<typename T>
Img<T>& Img<T>::divideByAlpha()
{
  fora(y, 0, h) fora(x, 0, w) {
    double alpha = operator()(x,y,alphaChannel)/(double)(getValuesPerChannel()-1);
    if (alpha > 0) fora(c, 0, ch) {
      if (c == alphaChannel) continue;
      operator()(x,y,c) = operator()(x,y,c)/alpha;
    }
  }
  return *this;
}

template<typename T>
Img<T> Img<T>::getChannel(int channel, int alphaChannel) const
{
  assert(channel >= 0 && channel < ch);
  Img img(w, h, 1, alphaChannel);
  fora(i, 0, w*h) img(i,0) = operator()(i,channel);
  return img;
}

template<typename T>
Img<T> Img<T>::getChannels(std::vector<int> channels, int alphaChannel) const
{
  forlist(i, channels) assert(channels[i] >= 0 && channels[i] <= ch);
  Img out(w, h, channels.size(), alphaChannel);
  fora(y,0,h) fora(x,0,w) {
    int c = 0;
    forlist(i, channels) {
      out(x,y,c) = operator()(x,y,channels[i]);
      c++;
    }
  }
  return out;
}

template<typename T>
Img<T> Img<T>::cloneChannels(int nChannelsFactor, int alphaChannel) const
{
  Img out(w, h, nChannelsFactor, alphaChannel);
  fora(y, 0, h) fora(x, 0, w) fora(c1, 0, ch) fora(c2, 0, nChannelsFactor) out(x,y,c2) = operator()(x,y,c1);
  return out;
}

template<typename T>
Img<T>& Img<T>::replaceChannels(const Img &inImg, std::vector<int> inImgChannels, std::vector<int> thisImgChannels)
{
  assert(inImgChannels.size() == thisImgChannels.size());
  forlist(i, inImgChannels) assert(inImgChannels[i] >= 0 && inImgChannels[i] <= inImg.ch);
  forlist(i, thisImgChannels) assert(thisImgChannels[i] >= 0 && thisImgChannels[i] <= ch);
  fora(y,0,h) fora(x,0,w) {
    forlist(i, inImgChannels) {
      operator()(x,y,thisImgChannels[i]) = inImg(x,y,inImgChannels[i]);
    }
  }
  return *this;
}

template<typename T>
Img<T>& Img<T>::swapChannels(int ch1, int ch2)
{
  fora(y, 0, h) fora(x, 0, w) {
    std::swap(operator()(x, y, ch1), operator()(x, y, ch2));
  }
  return *this;
}

template<typename T>
Img<T> Img<T>::getAlphaChannel() const
{
  return getChannel(alphaChannel, alphaChannel);
}

template<typename T>
Img<T>& Img<T>::replaceAlpha(const Img<T> &alpha)
{
  assert(w == alpha.w && h == alpha.h && alpha.ch == 1);
  fora(i, 0, w*h) operator()(i,alphaChannel) = alpha(i,0);
  return *this;
}

template<typename T>
void Img<T>::getBoundingBox(int &x1, int &y1, int &x2, int &y2, int channel, int minVal, int maxVal) const
{
  x1 = w-1; y1 = h-1; x2 = 0; y2 = 0;
  fora(y, 0, h) fora(x, 0, w) {
    const T& c = operator()(x, y, channel);
    if (c >= minVal && c <= maxVal) {
      if (x < x1) x1 = x;
      if (y < y1) y1 = y;
      if (x > x2) x2 = x;
      if (y > y2) y2 = y;
    }
  }
}

template<typename T>
Img<T> Img<T>::crop(int x1, int y1, int x2, int y2, int &modX1, int &modY1, int &modX2, int &modY2) const
{
  assert(x2 >= x1 && y2 >= y1);
  modX1 = x1 < 0 ? 0 : (x1 > this->w ? this->w-1 : x1);
  modY1 = y1 < 0 ? 0 : (y1 > this->h ? this->h-1 : y1);
  modX2 = x2 < 0 ? 0 : (x2 > this->w-1 ? this->w-1 : x2);
  modY2 = y2 < 0 ? 0 : (y2 > this->h-1 ? this->h-1 : y2);

  int w = modX2-modX1+1, h = modY2-modY1+1;
  return crop(w, h, modX1, modY1);
}

template<typename T>
Img<T> Img<T>::crop(int w, int h, int shiftX, int shiftY) const
{
  assert(w <= this->w && h <= this->h && shiftX >= 0 && shiftX+w <= this->w && shiftY >= 0 && shiftY+h <= this->h);
  Img img(w, h, ch, alphaChannel);
  fora(y, 0, h) fora(x, 0, w) fora(c, 0, ch) {
    img(x,y,c) = operator()(x+shiftX,y+shiftY,c);
  }
  return img;
}

template<typename T>
Img<T> Img<T>::resize(int w, int h, int shiftX, int shiftY, const Color<T> &color) const
{
  assert(w >= 0 && h >= 0);

  // no resize - return a copy
  if (w == this->w && h == this->h && shiftX == 0 && shiftY == 0) return deepCopy();

  // resize
  Img img(w, h, ch, alphaChannel);
  fora(y, 0, h) fora(x, 0, w) fora(c, 0, ch) {
    if (x-shiftX >= 0 && y-shiftY >= 0 && x-shiftX < this->w && y-shiftY < this->h) {
      img(x,y,c) = operator()(x-shiftX,y-shiftY,c);
    } else {
      // fill new areas with specified color
      if (c < color.ch) img(x,y,c) = color(c);
      else img(x,y,c) = color(0); // if color does not specify all channels, use colors(0) for the rest
    }
  }
  return img;
}

template<typename T>
Img<T> Img<T>::resize(int w, int h, int shiftX, int shiftY) const
{
    return resize(w, h, shiftX, shiftY, Color<T>({0}));
}

template<typename T>
Img<T>& Img<T>::blendWithInPlace(const Img<T> &topImg)
{
  assert(w == topImg.w && h == topImg.h && ch == topImg.ch && alphaChannel == topImg.alphaChannel);
  double maxVal = getValuesPerChannel();
  fora(y, 0, h) fora(x, 0, w) {
    double topAlpha = topImg(x,y,topImg.alphaChannel) / maxVal;
    double bottomAlpha = operator()(x,y,alphaChannel) / maxVal;
    fora(c, 0, ch) {
      if (c == alphaChannel) continue;
      const T& top = topImg(x,y,c);
      const T& bottom = operator()(x,y,c);
      operator()(x,y,c) = alphaBlending(bottom, top, topAlpha);
    }
    operator()(x,y,alphaChannel) = alphaBlending(bottomAlpha, topAlpha, topAlpha) * maxVal;
  }
  return *this;
}

template<typename T>
inline float Img<T>::gauss(float x, float sigma) {
  const float pi = 3.14159265358979323846;
  return 1.0 / (sigma * sqrtf(2 * pi)) * expf(-(x*x) / (2*(sigma*sigma)));
}

template<typename T>
std::vector<float> Img<T>::genGaussKernel(float sigma)
{
  const int M = 3 * ceil(sigma);
  std::vector<float> K(M+1);
  float sum = 0;
  fora(i, 0, M+1) {
    K[i] = gauss(i, sigma);
    sum += i>0 ? 2*K[i] : K[i];
  }
  fora(i, 0, M+1) K[i] /= sum;
  return K;
}

template<typename T>
Img<float> Img<T>::conv1D(const std::vector<float> &K, BoundaryHandling bndHandling)
{
  Img<float> imgOut; imgOut.initImage(*this, false);
  const int M = K.size()-1;
  #pragma omp parallel for
  fora(j, 0, h) fora(i, 0, w) {
    fora(c, 0, ch) imgOut(i,j,c) += K[0] * operator()(i,j,c);
    fora(k, i+1, i+M+1) {
      fora(c, 0, ch) {
        const T& i1 = operator()(k,j,c,bndHandling);
        const T& i2 = operator()(2*i-k,j,c,bndHandling);
        imgOut(i,j,c) += K[k-i] * (i1 + i2);
      }
    }
  }
  return imgOut;
}

template<typename T>
Img<T>& Img<T>::blur(float sigma)
{
  if (sigma > 0) {
    // generate kernel
    const std::vector<float> &K = genGaussKernel(sigma);

    // using separability property
    Img<float> convx = transpose().conv1D(K);
    Img<float> convxy = convx.transpose().conv1D(K).clamp(0.f, (float)(getValuesPerChannel()-1));
    cast(convxy);
  }
  return *this;
}

template<typename T>
Img<T>& Img<T>::invert(T maxVal)
{
  fora(y,0,h) fora(x,0,w) fora(c,0,ch) {
    if (c == alphaChannel) continue;
    operator()(x,y,c) = maxVal-operator()(x,y,c);
  }
  return *this;
}

template<typename T>
Img<T> Img<T>::transpose() const
{
  Img out(h, w, ch, alphaChannel);
  fora(y,0,h) fora(x,0,w) fora(c,0,ch) {
    out(y,x,c) = operator()(x,y,c);
  }
  return out;
}

template<typename T>
Img<T>& Img<T>::clamp(T min, T max)
{
  fora(y,0,h) fora(x,0,w) fora(c,0,ch) {
    operator()(x,y,c) = std::max(min, std::min(operator()(x,y,c), max));
  }
  return *this;
}

template<typename T>
bool Img<T>::equalData(const Img<T> &img) const
{
  if (w != img.w || h != img.h || ch != img.ch) return false;
  const int n = ch*w*h;
  fora(i, 0, n) if (data[i] != img.data[i]) return false;
  return true;
}

template<typename T>
bool Img<T>::equalDimension(const Img<T> &img) const
{
  if (w != img.w || h != img.h || ch != img.ch) return false;
  return true;
}

template<typename T>
void Img<T>::computeCentroid(double &cx, double &cy, T threshold)
{
  cx = 0; cy = 0;
  int countX = 0, countY = 0;
  fora(y,0,h) fora(x,0,w) {
    if (operator()(x,y,alphaChannel) > threshold) {
      cx += x; cy += y;
      countX++; countY++;
    }
  }
  cx /= countX; cy /= countY;
}

template<typename T>
Img<T> Img<T>::warp(const Img<T> &img, const Img<float> &Dx, const Img<float> &Dy, BoundaryHandling bndHandling, Sampling sampling)
{
  assert(img.w == Dx.w && img.h == Dx.h);
  assert(img.w == Dy.w && img.h == Dy.h);
  assert(Dx.ch == 1 && Dy.ch == 1);
  Img out(img.w, img.h, img.ch, img.alphaChannel);
  fora(y,0,img.h) fora(x,0,img.w) fora(c,0,img.ch) {
    float xPos = x + Dx(x,y,0);
    float yPos = y + Dy(x,y,0);
    out(x,y,c) = img.sample(xPos, yPos, c, bndHandling, sampling);
  }
  return out;
}

template<typename T>
Img<T> Img<T>::warp(const Img<float> &Dx, const Img<float> &Dy, BoundaryHandling bndHandling, Sampling sampling)
{
  return warp(*this, Dx, Dy, bndHandling, sampling);
}

template<typename T>
void Img<T>::getColorsUsage(const Img &img, std::vector<Color<T>> &colors, std::vector<int> &count, const Img &alphaChannel)
{
  using namespace std;
  map<Color<T>,int> usageMap;
  fora(y,0,img.h) fora(x,0,img.w) {
    if (!alphaChannel.isNull() && alphaChannel(x,y,0) == 0) continue;
    Color<T> color = img.getPixel(x,y);
    usageMap[color]++;
  }

  vector<pair<Color<T>,int>> usage;
  usage.insert(usage.end(), usageMap.begin(), usageMap.end());
  sort(usage.begin(), usage.end(), [](const auto &l, const auto &r) {
    return l.second > r.second;
  });

  colors.resize(usage.size());
  count.resize(usage.size());
  forlist(i, usage) {
    colors[i] = usage[i].first;
    count[i] = usage[i].second;
  }
}

template<typename T>
void Img<T>::getColorsUsage(std::vector<Color<T>> &colors, std::vector<int> &count, const Img &alphaChannel)
{
  getColorsUsage(*this, colors, count, alphaChannel);
}

template<typename T>
void Img<T>::getColorsUsage(const Img &img, std::vector<Color<T>> &colors, std::vector<int> &count)
{
  getColorsUsage(img, colors, count, Img());
}

template<typename T>
void Img<T>::getColorsUsage(std::vector<Color<T>> &colors, std::vector<int> &count)
{
  getColorsUsage(*this, colors, count);
}

template<typename T>
void Img<T>::reduceColors(const std::vector<Color<T>> &colors, const std::vector<int> &count, double threshold)
{
  return reduceColors(*this, colors, count, threshold);
}

template<typename T>
void Img<T>::reduceColors(const std::vector<Color<T>> &colors, const std::vector<int> &count, double threshold, int &maxColors)
{
  return reduceColors(*this, colors, count, threshold, maxColors);
}

template<typename T>
void Img<T>::reduceColors(Img &img, const std::vector<Color<T>> &colors, const std::vector<int> &count, double threshold)
{
  int maxColors;
  reduceColors(img, colors, count, threshold, maxColors);
}

template<typename T>
void Img<T>::reduceColors(Img &img, const std::vector<Color<T>> &colors, const std::vector<int> &count, double threshold, int &maxColors)
{
  using namespace std;
  int total = 0;
  maxColors = 0;
  forlist(i, count) total += count[i];
  forlist(i, count) {
    double r = count[i]/(double)total;
    if (r < threshold) {
      maxColors = i;
      break;
    }
  }
  reduceColors(img, colors, count, maxColors);
}

template<typename T>
void Img<T>::reduceColors(const std::vector<Color<T>> &colors, const std::vector<int> &count, int maxColors)
{
  return reduceColors(*this, colors, count, maxColors);
}

template<typename T>
void Img<T>::reduceColors(Img &img, const std::vector<Color<T>> &colors, const std::vector<int> &count, int maxColors)
{
  using namespace std;
  map<Color<T>,Color<T>> replacement;
  forlist(i, count) {
    const Color<T> &c1 = colors[i];
    Color<float> c1YUV; c1YUV.initColor(c1); RGBtoYUV(c1, c1YUV);
    if (i >= maxColors) {
      // for the current color, find the most similar color from ones that are allowed to be used
      double min = numeric_limits<double>::infinity();
      int id = -1;
      forlist(j, colors) {
        const Color<T> &c2 = colors[j];
        Color<float> c2YUV; c2YUV.initColor(c2); RGBtoYUV(c2, c2YUV);
        if (j >= maxColors) break;
        double dist = 0;
        fora(i, 0, c1.ch) dist += (c1YUV(i)-c2YUV(i)) * (c1YUV(i)-c2YUV(i));
        if (min > dist) { min = dist; id = j; }
      }
      replacement[colors[i]] = colors[id];
    }
  }

  replaceColors(img, replacement);
}

template<typename T>
void Img<T>::replaceColors(Img &img, const std::map<const Color<T>, Color<T> > &replacement)
{
  fora(y,0,img.h) fora(x,0,img.w) {
    if (img.alphaChannel > -1 && img(x,y,img.alphaChannel) == 0) continue;
    Color<T> p = img.getPixel(x,y);
    const Color<T> &q = replacement.begin()->first;
    const int n = std::min(p.ch, q.ch);
    const auto &itQ = replacement.find(p.head(n));
    if (itQ != replacement.end()) {
      Color<T> q = itQ->second;
      const int n = std::min(p.ch, q.ch);
      p.head(n) = q.head(n);
    }
  }
}

template<typename T>
void Img<T>::replaceColors(const std::map<const Color<T>, Color<T> > &replacement)
{
  replaceColors(*this, replacement);
}

template<typename T>
void Img<T>::minMax(std::vector<T> &min, std::vector<T> &max) const
{
  min.resize(ch);
  max.resize(ch);
  fora(c, 0, ch) {
    min[c] = std::numeric_limits<T>::max();
    max[c] = std::numeric_limits<T>::min();
  }
  fora(i, 0, wh()) {
    fora(c, 0, ch) {
      const float &val = operator()(i,c);
      if (val < min[c]) min[c] = val;
      if (val > max[c]) max[c] = val;
    }
  }
}

// incorrect
template<typename T>
Img<T> Img<T>::subsample(int subsampleFactor, Sampling sampling) const
{
  Img<T> out(w/subsampleFactor,h/subsampleFactor,ch,alphaChannel);
  fora(y, 0, out.h) fora(x, 0, out.w) fora(c, 0, ch) {
    out(x,y,c) = sample(x*subsampleFactor, y*subsampleFactor, c, none, sampling);
  }
  return out;
}

template<typename T>
bool Img<T>::checkBounds(int x, int y, int c) const
{
  return x >= 0 && y >= 0 && x < w && y < h && c >= 0 && c < ch;
}

template<typename T>
void Img<T>::updateColorData()
{
//  if (dataC != nullptr) delete[] dataC;
//  class Test {
////    unsigned char data[1];
//      unsigned char *t;
//      char a;
////      int a;
//  };
//  Test test;
//  dataC = new Color<T>[w*h];
////  dataC = (Color<Tstd::malloc(w*h*sizeof(Color<T>));
//  std::cout << sizeof(Color<T>) << std::endl;
//  std::cout << sizeof(Test) << std::endl;
//  fora(y,0,h) fora(x,0,w) {
//    Color<T> &c = dataC[y*w+x];
//    c.ch = ch;
//    c.sharedData = true;
//    c.data = &operator()(x,y,0);
//  }
}

template<typename T>
std::string Img<T>::debugPrefix()
{
  return std::string("Img<T=") + std::to_string(sizeof(T)) + ">";
}

template<typename T>
T Img<T>::bilinearInterpolation(T I0, T I1, T I2, T I3, double dx, double dy) {
  return (I0 * (1.0 - dx) + I1 * dx) * (1.0 - dy) + (I2 * (1.0 - dx) + I3 * dx) * dy;
}

template<typename T>
double Img<T>::alphaBlending(double A, double B, double BAlpha)
{
  return B + A*(1.0-BAlpha);
}

template<typename T>
std::ostream& operator<<(std::ostream &os, const Img<T> &img)
{
  os << std::string("Img<T=") + std::to_string(sizeof(T)) + ">" << ": w: " << img.w << ", h: " << img.h << ", ch: " << img.ch << ", alphaChannel: " << img.alphaChannel << ", sharedData: " << img.sharedData;
  return os;
}

template<typename T>
inline bool operator==(const Img<T> &I1, const Img<T> &I2)
{
  if (I1.alphaChannel != I2.alphaChannel || I1.sharedData != I2.sharedData) return false;
  return I1.equalData(I2);
}

template<typename T>
inline Img<T>& Img<T>::operator+=(const T &t)
{
  fora(i, 0, w*h*ch) data[i] += t;
  return *this;
}

template<typename T>
template<typename U>
inline Img<T>& Img<T>::operator-=(const U &t)
{
  fora(i, 0, w*h*ch) data[i] -= t;
  return *this;
}

template<typename T>
inline Img<T>& Img<T>::operator*=(const T &t)
{
  fora(i, 0, w*h*ch) data[i] *= t;
  return *this;
}

template<typename T>
inline Img<T>& Img<T>::operator/=(const T &t)
{
  fora(i, 0, w*h*ch) data[i] /= t;
  return *this;
}

template<typename T>
inline Img<T>& Img<T>::operator+=(const Img<T> &rhs)
{
  assert(w == rhs.w && h == rhs.h && ch == rhs.ch);
  fora(i, 0, w*h*ch) data[i] += rhs.data[i];
  return *this;
}

template<typename T>
inline Img<T>& Img<T>::operator-=(const Img<T> &rhs)
{
  assert(w == rhs.w && h == rhs.h && ch == rhs.ch);
  fora(i, 0, w*h*ch) data[i] -= rhs.data[i];
  return *this;
}

template<typename T>
inline Img<T>& Img<T>::operator*=(const Img<T> &rhs)
{
  assert(w == rhs.w && h == rhs.h && ch == rhs.ch);
  fora(i, 0, w*h*ch) data[i] *= rhs.data[i];
  return *this;
}

template<typename T>
inline Img<T>& Img<T>::operator/=(const Img<T> &rhs)
{
  assert(w == rhs.w && h == rhs.h && ch == rhs.ch);
  fora(i, 0, w*h*ch) data[i] /= rhs.data[i];
  return *this;
}

template<typename T>
inline Img<T> operator+(const Img<T> &lhs, const Img<T> &rhs)
{
  Img<T> out = lhs.deepCopy();
  out += rhs;
  return out;
}

template<typename T>
inline Img<T> operator-(const Img<T> &lhs, const Img<T> &rhs)
{
  Img<T> out = lhs.deepCopy();
  out -= rhs;
  return out;
}

template<typename T>
inline Img<T> operator*(const Img<T> &lhs, const Img<T> &rhs)
{
  Img<T> out = lhs.deepCopy();
  out *= rhs;
  return out;
}

template<typename T>
inline Img<T> operator/(const Img<T> &lhs, const Img<T> &rhs)
{
  Img<T> out = lhs.deepCopy();
  out /= rhs;
  return out;
}

template<typename T>
inline Img<T> operator+(const Img<T> &lhs, const T &c)
{
  Img<T> out = lhs.deepCopy();
  out += c;
  return out;
}

template<typename T,typename U>
inline Img<T> operator-(const Img<T> &lhs, const U &c)
{
  Img<T> out = lhs.deepCopy();
  out -= c;
  return out;
}

template<typename T>
inline Img<T> operator-(const T &c, const Img<T> &rhs)
{
  Img<T> out = rhs.deepCopy();
  fora(i, 0, out.w*out.h*out.ch) out.data[i] = c-out.data[i];
  return out;
}

template<typename T,typename U>
inline Img<T> operator*(const Img<T> &lhs, const U &c)
{
  Img<T> out = lhs.deepCopy();
  out *= c;
  return out;
}

template<typename T,typename U>
inline Img<T> operator/(const Img<T> &lhs, const U &c)
{
  Img<T> out = lhs.deepCopy();
  out /= c;
  return out;
}




// other functions
template<typename T,typename U>
inline void RGBtoYUV(const Color<T> &in, Color<U> &out)
{
  assert((in.ch == 3 || in.ch == 4) && out.ch == in.ch);
  const U& c0 = in(0);
  const U& c1 = in(1);
  const U& c2 = in(2);
  out(0) = 0.299 * c0 + 0.587 * c1 + 0.114 * c2;
  out(1) = -0.147 * c0 + -0.289 * c1 + 0.436 * c2;
  out(2) = 0.615 * c0 + -0.515 * c1 + -0.100 * c2;
}

template<typename T,typename U>
inline void YUVtoRGB(const Color<T> &in, Color<U> &out)
{
  assert((in.ch == 3 || in.ch == 4) && out.ch == in.ch);
  const U& c0 = in(0);
  const U& c1 = in(1);
  const U& c2 = in(2);
  out(0) = 1.0 * c0 + 0.0 * c1 + 1.137 * c2;
  out(1) = 1.0 * c0 + -0.397 * c1 + -0.580 * c2;
  out(2) = 1.0 * c0 + 2.034 * c1 + 0.0 * c2;
}

template<typename T,typename U>
void RGBtoYUV(const Img<T> &in, Img<U> &out)
{
  assert(in.w == out.w && in.h == out.h);
  fora(y,0,in.h) fora(x,0,in.w) {
    Color<T> cIn = in.getPixel(x,y);
    Color<U> cOut = out.getPixel(x,y);
    RGBtoYUV(cIn, cOut);
  }
}

template<typename T,typename U>
void YUVtoRGB(const Img<T> &in, Img<U> &out)
{
  assert(in.w == out.w && in.h == out.h);
  fora(y,0,in.h) fora(x,0,in.w) {
    Color<T> cIn = in.getPixel(x,y);
    Color<U> cOut = out.getPixel(x,y);
    YUVtoRGB(cIn, cOut);
  }
}

#endif // IMAGE_H
