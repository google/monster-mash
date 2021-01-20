// Copyright (c) 2014-2020 Czech Technical University in Prague
// Licensed under the MIT License.

#ifndef REGIONTOMESH_H
#define REGIONTOMESH_H

#include <Eigen/Core>
#include <vector>
#include <image/image.h>
#include <set>

class TriData
{
public:
  TriData() {};
  TriData(const std::vector<std::vector<Eigen::Vector2f>> &regionBnds,
          const std::vector<Eigen::Vector2f> &regionHolePts,
          const std::vector<Eigen::Vector2f> &interiorPts,
          const std::string &triangleOpts)
      : regionBnds(regionBnds), regionHolePts(regionHolePts), interiorPts(interiorPts), triangleOpts(triangleOpts) {}
  void scale(float s) {
    fora(i, 0, V.rows()) V.row(i) *= s;
    fora(i, 0, H.rows()) H.row(i) *= s;
    for(auto &i : regionBnds) for(auto &j : i) j *= s;
    for(auto &i : regionHolePts) i *= s;
    for(auto &i : interiorPts) i *= s;
  }
  std::vector<std::vector<Eigen::Vector2f>> regionBnds;
  std::vector<Eigen::Vector2f> regionHolePts;
  std::vector<Eigen::Vector2f> interiorPts;
  std::string triangleOpts;
  Eigen::MatrixXd V, H;
  Eigen::MatrixXi E;
  float scaling = 1;
};

void triangulate(TriData &triData, Eigen::MatrixXd &Vout, Eigen::MatrixXi &Fout);
void outlineToMesh(const std::vector<Imguc> &outlines, const std::vector<Imguc> &segs,
              const bool aboveOnly, const bool belowOnly, const bool interiorMergingPtsOnly,
              const std::string &triangleOpts, std::set<int> &mergeBothSides, std::vector<Eigen::MatrixXd> &Vs, std::vector<Eigen::MatrixXi> &Fs, const int smoothFactor);
void outlineToMesh(const std::vector<Imguc> &outlines, const std::vector<Imguc> &segs,
              const bool aboveOnly, const bool belowOnly, const bool interiorMergingPtsOnly,
              const std::string &triangleOpts, std::set<int> &mergeBothSides, std::vector<Eigen::MatrixXd> &Vs, std::vector<Eigen::MatrixXi> &Fs,
              const int smoothFactor,
              std::vector<std::vector<std::vector<Eigen::Vector2f>>> &regionsBnds,
              std::vector<TriData> &triData);
void findRegionBoundary(const Imguc &S, std::vector<std::vector<Eigen::Vector2f>> &bnds, std::vector<Eigen::Vector2f> &holePts);
void outlineToRegion(const Imguc &Io, Imguc &Ir);
void regionToOutline(Imguc &Ir, Imguc &Io, bool keepSingleRegion = true, bool addBoundaryToRegion = false);

#endif // REGIONTOMESH_H
