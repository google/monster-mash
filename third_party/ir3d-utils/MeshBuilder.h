// Copyright (c) 2014-2020 Czech Technical University in Prague
// Licensed under the MIT License.

#ifndef MESHBUILDER_H
#define MESHBUILDER_H

#include <string>
#include <vector>
#include <Eigen/Dense>
#include <unordered_map>
#include <map>
#include <set>

class MeshBuilder {
public:
  std::vector<Eigen::MatrixXd> Vs;
  std::vector<Eigen::MatrixXi> Fs;
  std::vector<std::vector<int>> bnds, neumannBnds;
  std::vector<std::vector<int>> mergeBnds;
  std::vector<std::vector<std::vector<int>>> allAnnotations;
  std::vector<std::vector<int>> partComponents;

  Eigen::MatrixXd Vc; // complete
  Eigen::MatrixXi Fc; // complete
  std::vector<int> bndc; // complete
  std::vector<int> neumannBndc; // complete
  std::vector<int> classes;

  std::vector<std::vector<int>> mergeVs;
  std::vector<std::vector<int>> mergeFs;

  int Z_COORD;
  double scaling;
  double spacing;
  int counter;

  void parseAnnotations(int ann, std::vector<int> &annParsed);

  template<typename ValueT> class mymap;

public:
  enum VertexType { all=-1, bnd=0, in=1 };
  struct V2VCorrCandidate
  {
    VertexType type;
    int ind;
    Eigen::Vector3d coord;
    int customNum;
  };
  struct V2VCorr // vertex to vertex correspondence
  {
    VertexType typea;       // type(0=bnd,1=inside)
    VertexType typeb;       // type(0=bnd,1=inside)
    int a;                  // index of vertex a
    int b;                  // index of vertex b
    int customNum;          // customNum
    int subseqent;
  };

  std::vector<std::string> filenames;
  std::vector<std::vector<std::tuple<int,int,bool>>> bconds_all;
  std::vector<int> pits;
  std::vector<std::vector<std::pair<int,int>>> graftingCorr;
  std::vector<std::vector<std::vector<std::pair<int,int>>>> mergingCorr; //a,b
  std::vector<std::vector<std::vector<V2VCorr>>> eqCorr, ineqCorr;
  std::vector<std::vector<int>> graftingCorrN;
  bool neumannAtGraftingBnd = false;
  bool graftingBndStitching = false;
  bool graftingBndIneq = false;
  std::vector<int> bndL;
  std::vector<std::vector<int>> parts1;
  std::vector<int> partsc1; // complete
  std::vector<std::vector<std::string>> boundaryConditions;
  std::vector<bool> flipReg;


  MeshBuilder();
  MeshBuilder(int Z_COORD, double spacing, double scaling);
  void processPlanarMesh(Eigen::MatrixXd &V, Eigen::MatrixXi &F, const bool computeBnds, std::vector<std::vector<int>> &customBnds, const bool removeUnreferenced);
  void processTwoSidedMesh(Eigen::MatrixXd &Vin1, Eigen::MatrixXi &Fin1, Eigen::MatrixXd &Vin2, Eigen::MatrixXi &Fin2, const bool computeBnds, std::vector<std::vector<int>> &customBnds, const bool removeUnreferenced);
  void createCompleteMesh();
  void findCorrespondences(const std::vector<V2VCorrCandidate> &candidates, const Eigen::MatrixXd &mesh, std::vector<V2VCorr> &corr, int maxindex, const std::vector<int> &part, const std::vector<bool> &isBnd);
  void findBndCandidates(std::vector<std::vector<V2VCorrCandidate> > &ineqCandidates,
                         std::vector<std::vector<V2VCorrCandidate> > &eqCandidates,
                         std::vector<std::vector<V2VCorrCandidate> > &graftingBndCandidates);
  void loadPlanarMesh(std::string nameofPartWihtoutSuffix, std::string suffix);
  void duplicateBnd(const Eigen::Vector3d &w);
  void getMesh(int num, Eigen::MatrixXd &V, Eigen::MatrixXi &F);
  void getBoundary(int num, std::vector<int> &bnd, std::vector<int> &neumannBnd);
  void getCompleteMesh(Eigen::MatrixXd &V, Eigen::MatrixXi &F);
  void getCompleteBoundary(std::vector<int> &bnd, std::vector<int> &neumannBnd);
  void getCompleteBoundary(std::vector<int> &bnd);
  void getClasses(std::vector<int> &classes);
  void getAnnotationsForVertices(std::vector<std::vector<int>> &annotations);
  int getMeshesCount();
  static void mirrorMesh(const Eigen::MatrixXd &inV, const Eigen::MatrixXi &inF, const Eigen::MatrixXd &inV2, const Eigen::MatrixXi &inF2, const std::vector<int> &bnd, const std::vector<int> &neumannBnd, Eigen::MatrixXd &outV, Eigen::MatrixXi &outF, std::vector<int> &part1, std::vector<int> &part2, std::vector<int> &part2NeumannBnd);
  void mergeMesh(const std::vector<std::pair<int,int>> &corr, bool reverseN = false);
  static void meshMergeAndRemoveDuplicates(const Eigen::MatrixXd &inV, const Eigen::MatrixXi &inF, const std::unordered_map<int, std::vector<int> > &verticesToMerge, Eigen::MatrixXd &outV, Eigen::MatrixXi &outF, std::vector<std::vector<int>> &inVerticesOfParts, std::vector<std::vector<int> > &outVerticesOfParts, std::vector<bool> &removeV, std::vector<int> &reindex);
  static void meshMergeAndRemoveDuplicates(const Eigen::MatrixXd &inV, const Eigen::MatrixXi &inF, const std::vector<std::pair<int,int>> &verticesToMerge, Eigen::MatrixXd &outV, Eigen::MatrixXi &outF, std::vector<std::vector<int>> &inVerticesOfParts, std::vector<std::vector<int> > &outVerticesOfParts, std::vector<bool> &removeV, std::vector<int> &reindex);
  static void meshMergeAndRemoveDuplicates(const Eigen::MatrixXd &inV, const Eigen::MatrixXi &inF, const std::vector<std::pair<int,int>> &verticesToMerge, Eigen::MatrixXd &outV, Eigen::MatrixXi &outF, std::vector<std::vector<int>> &inVerticesOfParts, std::vector<std::vector<int> > &outVerticesOfParts);
  static void meshMergeAndRemoveDuplicates(const Eigen::MatrixXd &inV, const Eigen::MatrixXi &inF, const std::unordered_map<int,std::set<int>> &verticesToMerge, Eigen::MatrixXd &outV, Eigen::MatrixXi &outF, std::vector<std::vector<int>> &inVerticesOfParts, std::vector<std::vector<int>> &outVerticesOfParts, std::vector<bool> &removeV, std::vector<int> &reindex);
  void getBndAndStart(int partId, std::vector<std::vector<int>> &partBnd, std::vector<int> &starts);
};

#endif
