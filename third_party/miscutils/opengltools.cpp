#include <iostream>
#include "opengltools.h"
#include <miscutils/fsutils.h>
#include <unsupported/Eigen/OpenGLSupport>

using namespace std;
using namespace Eigen;

void GLMeshInitBuffers(GLMeshData &data)
{
  // init buffers for meshes
  glGenVertexArraysOES(1, &data.VAO);
  glGenBuffers(1, &data.VBO_V);
  glGenBuffers(1, &data.VBO_N);
  glGenBuffers(1, &data.VBO_T);
  glGenBuffers(1, &data.VBO_C);
  glGenBuffers(1, &data.VBO_PARTID);
  glGenBuffers(1, &data.VBO_F);
}

void GLMeshFillBuffers(GLuint program, GLMeshData &data, const MatrixXd &V, const MatrixXi &F, const MatrixXd &N, const MatrixXd &T, const MatrixXd &C, const MatrixXi &PARTID)
{
  // convert input matrices for usage in OpenGL
  data.V_converted = V.cast<float>();
  data.F_converted = F.cast<unsigned int>();
  data.N_converted = N.cast<float>();
  data.T_converted = T.cast<float>();
  data.C_converted = C.cast<float>();
  data.PARTID_converted = PARTID.cast<int>();

  // fill buffers
  glBindVertexArrayOES(data.VAO);
  // position
  GLint id;
  glBindBuffer(GL_ARRAY_BUFFER, data.VBO_V);
  glBufferData(GL_ARRAY_BUFFER, data.V_converted.size() * sizeof(float), data.V_converted.data(), GL_DYNAMIC_DRAW);
  id = glGetAttribLocation(program, "position");
  glVertexAttribPointer(id, data.V_converted.cols(), GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(id);
  // normal
  if (N.size() > 0) {
    glBindBuffer(GL_ARRAY_BUFFER, data.VBO_N);
    glBufferData(GL_ARRAY_BUFFER, data.N_converted.size() * sizeof(float), data.N_converted.data(), GL_DYNAMIC_DRAW);
    id = glGetAttribLocation(program, "normal");
    glVertexAttribPointer(id, data.N_converted.cols(), GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(id);
  }
  // texture coords
  if (T.size() > 0) {
    glBindBuffer(GL_ARRAY_BUFFER, data.VBO_T);
    glBufferData(GL_ARRAY_BUFFER, data.T_converted.size() * sizeof(float), data.T_converted.data(), GL_DYNAMIC_DRAW);
    id = glGetAttribLocation(program, "texCoord");
    glVertexAttribPointer(id, data.T_converted.cols(), GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(id);
  }
  // color
  if (C.size() > 0) {
    glBindBuffer(GL_ARRAY_BUFFER, data.VBO_C);
    glBufferData(GL_ARRAY_BUFFER, data.C_converted.size() * sizeof(float), data.C_converted.data(), GL_DYNAMIC_DRAW);
    id = glGetAttribLocation(program, "color");
    glVertexAttribPointer(id, data.C_converted.cols(), GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(id);
  }
  // faces
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.VBO_F);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.F_converted.size() * sizeof(unsigned int), data.F_converted.data(), GL_DYNAMIC_DRAW);
}

void GLMeshDestroyBuffers(GLMeshData &data)
{
  // destroy buffers
  glDeleteVertexArrays(1, &data.VAO);
  glDeleteBuffers(1, &data.VBO_V);
  glDeleteBuffers(1, &data.VBO_N);
  glDeleteBuffers(1, &data.VBO_T);
  glDeleteBuffers(1, &data.VBO_C);
  glDeleteBuffers(1, &data.VBO_PARTID);
  glDeleteBuffers(1, &data.VBO_F);
}

void GLMeshDraw(GLMeshData &data, GLuint type)
{
  // draw
  glBindVertexArrayOES(data.VAO);
  if (type == GL_LINES) {
    glDrawElements(type, 2*data.F_converted.rows(), GL_UNSIGNED_INT, 0);
  } else {
    glDrawElements(type, 3*data.F_converted.rows(), GL_UNSIGNED_INT, 0);
  }
}

void rasterizeGPUClear()
{
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE); // https://stackoverflow.com/questions/17224879/opengl-alpha-blending-issue-blending-ignored-maybe
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void uploadCameraMatrices(GLuint shader, const Eigen::Matrix4d &P, const Eigen::Matrix4d &M, const Eigen::Matrix4d &normalMatrix)
{
  glUseProgram(shader);
  glUniform(glGetUniformLocation(shader, "view"), M.cast<float>());
  glUniform(glGetUniformLocation(shader, "proj"), P.cast<float>());
  glUniform(glGetUniformLocation(shader, "normalMatrix"), normalMatrix.cast<float>());
}

GLuint loadShaders(const char *vertexShaderSrc, const char *fragmentShaderSrc){
  // create shaders
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

  // compile vertex shader and check its status
  glShaderSource(vertexShader, 1, &vertexShaderSrc, NULL);
  glCompileShader(vertexShader);
  auto checkCompileStatus = [](const string &prefix, GLuint shader) {
    GLint res = GL_FALSE;
    int length;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &res);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
      string msg; msg.resize(length+1);
      glGetShaderInfoLog(shader, length, NULL, &msg[0]);
      fprintf(stderr, "%s%s\n", prefix.c_str(), msg.c_str());
    }
  };
  checkCompileStatus("vertex shader error: ", vertexShader);

  // compile fragment shader and check its status
  glShaderSource(fragmentShader, 1, &fragmentShaderSrc, NULL);
  glCompileShader(fragmentShader);
  checkCompileStatus("fragment shader error: ", fragmentShader);

  // link program and check its status
  GLuint program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glLinkProgram(program);
  GLint res = GL_FALSE;
  int length;
  glGetProgramiv(program, GL_LINK_STATUS, &res);
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
  if (length > 0) {
    string msg; msg.resize(length+1);
    glGetProgramInfoLog(program, length, NULL, &msg[0]);
    fprintf(stderr, "%s%s\n", "program linking error: ", msg.c_str());
  }

  glDetachShader(program, vertexShader);
  glDetachShader(program, fragmentShader);
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return program;
}

GLuint loadShadersFromFile(const std::string &vertexShaderFn, const std::string &fragmentShaderFn)
{
  string vertexShaderSrc, fragmentShaderSrc;
  bool res = true;
  if (!read_entire_file(vertexShaderFn, vertexShaderSrc)) {
    cerr << "loadShadersFromFile: Error reading " << vertexShaderFn << endl;
    res = false;
  }
  if (!read_entire_file(fragmentShaderFn, fragmentShaderSrc)) {
    cerr << "loadShadersFromFile: Error reading " << fragmentShaderFn << endl;
    res = false;
  }
  if (res) return loadShaders(vertexShaderSrc.c_str(), fragmentShaderSrc.c_str());
  else return glCreateProgram();
}

void loadTexturesToGPU(const std::deque<std::string> &fn, std::vector<GLuint> &textureNames)
{
  const int num = fn.size();
  textureNames.resize(num);
  glGenTextures(num, textureNames.data());
  fora(i, 0, num) {
    glBindTexture(GL_TEXTURE_2D, textureNames[i]);
    // set the texture wrapping/filtering options (on the currently bound texture object)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // load and generate the texture
    Imguc I = Imguc::loadImage(fn[i], -1, 3);
    if (!I.isNull()) {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, I.w, I.h, 0, GL_RGB, GL_UNSIGNED_BYTE, I.data);
    } else {
      cerr << "Failed to load texture" << endl;
    }
  }
}

void loadTextureToGPU(const Imguc &I, GLuint &textureName)
{
  glGenTextures(1, &textureName);
  glBindTexture(GL_TEXTURE_2D, textureName);
  // set the texture wrapping/filtering options (on the currently bound texture object)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // load and generate the texture
  if (I.ch < 3) {
    cerr << I << endl;
    cerr << "Image with at least 3 channels required." << endl;
    return;
  }

  Imguc I2(I.w, I.h, 4, 3);;
  if (I.ch == 3) {
    // convert RGB to RGBA
    fora(y, 0, I.h) fora(x, 0, I.w) {
      fora(i, 0, 3) I2(x,y,i) = I(x,y,i);
      I2(x,y,3) = 255;
    }
  } else {
    I2 = I;
  }

  if (!I2.isNull()) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, I2.w, I2.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, I2.data);
  }
}
