#ifndef OPENGLTOOLS_H
#define OPENGLTOOLS_H

#include <deque>
#include <Eigen/Dense>
#include <image/image.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL_opengles2.h>

struct GLMeshData
{
  GLuint VAO, VBO_V, VBO_N, VBO_T, VBO_C, VBO_PARTID, VBO_F;
  // matrices converted for usage in OpenGL
  Eigen::Matrix<float,Eigen::Dynamic,Eigen::Dynamic,Eigen::RowMajor> V_converted, N_converted, T_converted, C_converted;
  Eigen::Matrix<int,Eigen::Dynamic,Eigen::Dynamic,Eigen::RowMajor> PARTID_converted;
  Eigen::Matrix<unsigned int,Eigen::Dynamic,Eigen::Dynamic,Eigen::RowMajor> F_converted;
};

void GLMeshInitBuffers(GLMeshData &data);
void GLMeshFillBuffers(GLuint program, GLMeshData &data, const Eigen::MatrixXd &V, const Eigen::MatrixXi &F,
                       const Eigen::MatrixXd &N = Eigen::MatrixXd(), const Eigen::MatrixXd &T = Eigen::MatrixXd(), const Eigen::MatrixXd &C = Eigen::MatrixXd(), const Eigen::MatrixXi &PARTID = Eigen::MatrixXi());
void GLMeshDestroyBuffers(GLMeshData &data);
void GLMeshDraw(GLMeshData &data, GLuint type);
void rasterizeGPUClear();
void uploadCameraMatrices(GLuint shader, const Eigen::Matrix4d &P, const Eigen::Matrix4d &M, const Eigen::Matrix4d &normalMatrix);
GLuint loadShaders(const char *vertexShaderSrc, const char *fragmentShaderSrc);
GLuint loadShadersFromFile(const std::string &vertexShaderFn, const std::string &fragmentShaderFn);
void loadTexturesToGPU(const std::deque<std::string> &fn, std::vector<GLuint> &textureNames);
void loadTextureToGPU(const Imguc &I, GLuint &textureName);

#endif // OPENGLTOOLS_H
