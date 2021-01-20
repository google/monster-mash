// Copyright 2020-2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "reconstruction.h"

#include <Eigen/src/SparseCore/SparseSolverBase-mod.h>
#include <igl/cotmatrix.h>
#include <igl/invert_diag.h>
#include <igl/massmatrix.h>
#include <igl/min_quad_with_fixed.h>
#include <image/imageUtils.h>
#include <ir3d-utils/MeshBuilder.h>
#include <ir3d-utils/regionToMesh.h>

#include <Eigen/Core>
#include <Eigen/Sparse>

#include "loadsave.h"
#include "macros.h"

#define ENABLE_MERGING_COMMON_BOUNDARY_IN_PREPROCESS

using namespace std;
using namespace Eigen;
using namespace igl;

template <typename T>
int sgn(T val) {
  return (T(0) < val) - (val < T(0));
}

bool reconstruction(
    const RecData &recData, const ImgData &imgData,
    const std::vector<Imguc> &outlineImgs, const std::vector<Imguc> &regionImgs,
    const std::string &triangleOpts, Eigen::MatrixXd &Vout,
    Eigen::MatrixXi &Fout, Eigen::MatrixXd &VPreinfOut,
    std::vector<std::vector<int>> &verticesOfParts,
    Eigen::SparseMatrix<double> &I, Eigen::SparseMatrix<double> &LFlat,
    Eigen::SparseMatrix<double> &LPreinf, Eigen::SparseMatrix<double> &LFinal,
    Eigen::SparseMatrix<double> &MFinal, Eigen::SparseMatrix<double> &MinvFinal,
    Eigen::SparseMatrix<double> &Aeq, Eigen::VectorXd &Beq,
    Eigen::SparseMatrix<double> &Aieq, Eigen::VectorXd &Bieq,
    std::vector<std::tuple<int, int, int>> &ineqRegionConds,
    std::vector<std::vector<int>> &bnds, std::vector<int> &mergeBnd,
    std::vector<std::tuple<int, int, int>> &mergeArmpitsCorrs,
    std::vector<TriData> &triData, const int smoothFactor,
    const double defaultInflationAmount, const bool armpitsStitching,
    const bool armpitsStitchingInJointOptimization,
    std::set<int> &mergeBothSides) {
  auto &regionInflationAmount = recData.regionInflationAmount;
  auto &layers = imgData.layers;

  const int N = regionImgs.size();
  const int numRegions = 2 * N;

  // convert regions into flat meshes
  vector<MatrixXd> VsFront, VsBack;
  vector<MatrixXi> FsFront, FsBack;
  vector<vector<vector<Vector2f>>> regionsBnds;
  const bool interiorMergingPtsOnly = true;
  outlineToMesh(outlineImgs, regionImgs, true, false, interiorMergingPtsOnly,
                triangleOpts, mergeBothSides, VsFront, FsFront, smoothFactor,
                regionsBnds, triData);
  outlineToMesh(outlineImgs, regionImgs, false, true, interiorMergingPtsOnly,
                triangleOpts, mergeBothSides, VsBack, FsBack, smoothFactor);

  // create vectors of indices of boundary points (customBnds) from regionsBnds
  vector<vector<vector<int>>> customBnds;
  forlist(k, regionsBnds) {
    vector<vector<int>> bnd1;
    int c = 0;
    forlist(l, regionsBnds[k]) {
      vector<int> bnd2;
      forlist(m, regionsBnds[k][l]) {
        bnd2.push_back(c);
        c++;
      }
      bnd1.push_back(bnd2);
    }
    customBnds.push_back(bnd1);
  }

  {
    int count = 0;
    forlist(i, VsFront) count += VsFront[i].rows();
    forlist(i, VsBack) count += VsBack[i].rows();
    DEBUG_CMD_MM(cout << endl
                      << "-- TRACE: total vertices: " << count << endl
                      << endl;)
  }

  // process all planar meshes
  const int Z_COORD = 2;
  MeshBuilder mb;
  mb.graftingBndStitching = false;
  mb.graftingBndIneq = false;
  mb.flipReg.resize(2 * N, false);
  forlist(i, regionImgs) {
#ifdef ENABLE_MERGING_COMMON_BOUNDARY_IN_PREPROCESS
    mb.processTwoSidedMesh(VsFront[i], FsFront[i], VsBack[i], FsBack[i], false,
                           customBnds[i], true);
#else
    mb.processPlanarMesh(VsFront[i], FsFront[i], false, customBndsFront[i],
                         false);
    mb.processPlanarMesh(VsBack[i], FsBack[i], false, customBndsBack[i], false);
#endif
  }
  // default inflation amount
  vector<float> inflationAmount(2 * N, defaultInflationAmount);
  forlist(i, regionImgs) {
    const int regId = layers[i];
    mb.flipReg[2 * i + 1] = true;
    auto it = regionInflationAmount.find(regId);
    if (it != regionInflationAmount.end()) {
      inflationAmount[2 * i] = inflationAmount[2 * i + 1] = it->second;
    }
    inflationAmount[2 * i + 1] *= -1;
  }
  mb.createCompleteMesh();
  bnds = mb.bnds;

  // Determine relative depth conditions for a region B based on the following
  // rules:
  // - for all regions A that are behind B, set B_backBnd is in front of
  // A_frontIn
  // - for all regions C that are in front of B, set B_frontBnd is behind
  // C_backIn Determine merging of a region B to the nearest one based on the
  // following rules: (If a region B has merging boundaries, it is supposed to
  // be connected with other regions, also we assume that it is connected to the
  // nearest region.)
  // - find the nearest region A (e.g. body) that is behind B (e.g. front leg)
  // and merge B_frontConnBnd with A_frontConnIn
  // - find the nearest region C (e.g. body) that is in front of B (e.g. back
  // leg) and merge B_backConnBnd with C_backConnIn
  typedef MeshBuilder::VertexType VertexType;
  vector<bool> &flipReg = mb.flipReg;
  vector<tuple<int, int, int, int>> selectedVCondsIneq, selectedVCondsEq;
  vector<vector<pair<int, int>>> selectedMergingCorrs;
  vector<bool> reverseNs;
  unordered_map<int, bool> vertexUsedMerge;
  set<int> vertexUsed;
  vector<tuple<int, int, int>> mergingPartNum;  // this part, opposite part

  fora(shift, 1, numRegions) {
    fora(B, 0, numRegions) {
      const bool BIsFrontSide = !flipReg[B];
      int A = B - shift;
      if (A >= 0) {  // A is behind B
        if (A == (B - 1) && A % 2 == 0)
          continue;  // A is front side of B, skip it
        const bool AIsFrontSide = !flipReg[A];
        if (BIsFrontSide && AIsFrontSide) {  // MERGING: A is behind B &&
                                             // BIsFrontSide && AIsFrontSide
          const auto &mc = mb.mergingCorr[B][A];
          vector<pair<int, int>> tmp;
          forlist(i, mc) {
            const int vInd = mc[i].second;  // boundary vertex
            if (vertexUsedMerge.find(vInd) == vertexUsedMerge.end()) {
              tmp.push_back(mc[i]);
              vertexUsedMerge[vInd] = true;
            }
          }
          selectedMergingCorrs.push_back(tmp);
          reverseNs.push_back(false);

          if (mergeBothSides.find(B / 2) != mergeBothSides.end()) {
            const auto &mc = mb.mergingCorr[B + 1][A + 1];
            vector<pair<int, int>> tmp;
            forlist(i, mc) {
              const int vInd = mc[i].second;  // boundary vertex
              if (vertexUsedMerge.find(vInd) == vertexUsedMerge.end()) {
                tmp.push_back(mc[i]);
                vertexUsedMerge[vInd] = true;
              }
            }
            selectedMergingCorrs.push_back(tmp);
            reverseNs.push_back(true);
          }

          mergingPartNum.push_back(
              make_tuple(B, B + 1, 1));  // BEWARE! hardcoded
        }

        // Inequalities for joint optimization:
        // for interior to interior correspondences
        //          ineqRegionConds.push_back(make_tuple(B,A,1));

        // for boundary to interior correspondences
        if (BIsFrontSide &&
            AIsFrontSide) {  // REL_DEPTHS: A is behind B && BIsFrontSide &&
                             // AIsFrontSide: assume merged boundary, only front
                             // bnd remains
          ineqRegionConds.push_back(make_tuple(B, A, 1));
        }
      }
      int C = B + shift;
      if (C < numRegions) {  // C is in front of B
        if (C == (B + 1) && C % 2 == 1)
          continue;  // C is back side of B, skip it
        const bool CIsFrontSide = !flipReg[C];
        if (!BIsFrontSide && !CIsFrontSide) {  // MERGING: C is in front of B &&
                                               // BIsBackSide && CIsBackSide
          const auto &mc = mb.mergingCorr[B][C];
          vector<pair<int, int>> tmp;
          forlist(i, mc) {
            const int vInd = mc[i].second;  // boundary vertex
            if (vertexUsedMerge.find(vInd) == vertexUsedMerge.end()) {
              tmp.push_back(mc[i]);
              vertexUsedMerge[vInd] = true;
            }
          }
          selectedMergingCorrs.push_back(tmp);
          mergingPartNum.push_back(
              make_tuple(B, B - 1, -1));  // BEWARE! hardcoded
          reverseNs.push_back(true);
        }

        if (BIsFrontSide && !CIsFrontSide) {  // REL_DEPTHS: C is in front of B
                                              // && BIsFrontSide && CIsBackSide
          ineqRegionConds.push_back(make_tuple(B, C, -1));
        }
      }
    }
  }

  // perform exterior (front to front) side merging
  int mergingCorrsNum = 0;
  forlist(i, selectedMergingCorrs) {
    mergingCorrsNum += selectedMergingCorrs[i].size();
    mb.mergeMesh(selectedMergingCorrs[i], reverseNs[i]);
    for (auto &el : selectedMergingCorrs[i]) {
      const int bndId = el.second;
      const int meshId = el.first;
      mergeBnd.push_back(bndId);
      mergeBnd.push_back(meshId);
    }
  }
  DEBUG_CMD_MM(cout << "mergingCorrs: " << mergingCorrsNum << endl;)

  // get the final flat mesh
  MatrixXd V;
  MatrixXi F;
  vector<int> bnd, neumannBnd;
  mb.getCompleteMesh(V, F);
  mb.getCompleteBoundary(bnd, neumannBnd);

  // convert relative depth conditions into a matrix form suitable for quadratic
  // programming
  struct m2mCorr  // mesh to mesh correspondence
  {
    VertexType typea;  // type(0=bnd,1=inside)
    VertexType typeb;  // type(0=bnd,1=inside)
    int a;
    int b;
    int customNum;  // customNum
    int sign;
  };
#ifdef ENABLE_MERGING_COMMON_BOUNDARY_IN_PREPROCESS
  vector<m2mCorr> eqs;
#else
  vector<m2mCorr> eqs(N);
  fora(i, 0, N) {
    eqs.push_back(
        m2mCorr{VertexType::bnd, VertexType::bnd, 2 * i, 2 * i + 1, 0, 0});
  }
#endif

  // equalities
  int num = 0;
  int numBndA = 0, numInA = 0, numBndB = 0, numInB = 0, numSkipped = 0;
  forlist(k, eqs) {
    const auto &el = eqs[k];
    const auto &bc = mb.eqCorr[el.a][el.b];
    forlist(i, bc) {
      const int customNum = el.customNum;
      const int vertexCustomNum = bc[i].customNum;
      // only apply the condition if customNum==0 or customNum==vertexCustomNum
      if (!(customNum == 0 || customNum == vertexCustomNum)) continue;
      if ((el.typea == VertexType::all && el.typeb == VertexType::all) ||
          (el.typea == VertexType::all && el.typeb == bc[i].typeb) ||
          (el.typea == bc[i].typea && el.typeb == VertexType::all) ||
          (el.typea == bc[i].typea && el.typeb == bc[i].typeb)) {
        if (el.typea == VertexType::all || el.typea == VertexType::bnd)
          numBndA++;
        if (el.typea == VertexType::all || el.typea == VertexType::in) numInA++;
        if (el.typeb == VertexType::all || el.typeb == VertexType::bnd)
          numBndB++;
        if (el.typeb == VertexType::all || el.typeb == VertexType::in) numInB++;
        num++;
      }
    }
    DEBUG_CMD_MM(cout << "eqs: " << el.a << " " << el.b << " typea:" << el.typea
                      << " typeb:" << el.typeb << endl;)
  }
  num += selectedVCondsEq.size();
  DEBUG_CMD_MM(cout << "eqsnum: " << num << ", bndA: " << numBndA << ", inA: "
                    << numInA << ", bndB: " << numBndB << ", inB: " << numInB
                    << ", skipped: " << numSkipped << endl;)
  Beq = VectorXd(num);
  Beq.fill(0);
  Aeq = SparseMatrix<double>(num, V.rows());
  int c = 0;
  unordered_map<int, set<int>> verticesToMerge;
  vector<pair<int, int>> bnd2bnd;
  //  set<int> verticesToMergeUsed;
  forlist(k, eqs) {
    const auto &el = eqs[k];
    const auto &bc = mb.eqCorr[el.a][el.b];
    forlist(i, bc) {
      const int customNum = el.customNum;
      const int vertexCustomNum = bc[i].customNum;
      if (!(customNum == 0 || customNum == vertexCustomNum)) continue;
      if (!((el.typea == VertexType::all && el.typeb == VertexType::all) ||
            (el.typea == VertexType::all && el.typeb == bc[i].typeb) ||
            (el.typea == bc[i].typea && el.typeb == VertexType::all) ||
            (el.typea == bc[i].typea && el.typeb == bc[i].typeb)))
        continue;

      verticesToMerge[bc[i].a].insert(bc[i].b);
      bnd2bnd.push_back(make_pair(bc[i].a, bc[i].b));
      //      verticesToMergeUsed.insert(bc[i].b);
      Aeq.insert(c, bc[i].a) = 1;
      Aeq.insert(c, bc[i].b) = -1;
      c++;
    }
  }
  {
    forlist(i, selectedVCondsEq) {
      const auto &s = selectedVCondsEq[i];
      const auto &bc = mb.eqCorr[get<0>(s)][get<1>(s)][get<2>(s)];
#ifdef ENABLE_EQUALITIES
      verticesToMerge.push_back(
          make_pair(bc.a, bc.b));  // TODO: this makes duplicates and causes
                                   // crashes later in merging phase
#endif
      Aeq.insert(c, bc.a) = 1;   // boundary
      Aeq.insert(c, bc.b) = -1;  // mesh
      c++;
    }
  }

  // inequalities
  num = selectedVCondsIneq.size();
  Bieq = VectorXd(num);
  Bieq.fill(0);
  Aieq = SparseMatrix<double>(num, V.rows());
  {
    int c = 0;
    forlist(i, selectedVCondsIneq) {
      const auto &s = selectedVCondsIneq[i];
      const auto &bc = mb.ineqCorr[get<0>(s)][get<1>(s)][get<2>(s)];
      const int sign = get<3>(s);
      Aieq.insert(c, bc.a) = sign * 1;   // boundary
      Aieq.insert(c, bc.b) = sign * -1;  // mesh
      c++;
    }
  }
  DEBUG_CMD_MM(cout << "ineqsNum: " << num << endl;)

  // preinflation
  DEBUG_CMD_MM(cout << "Inflation " << endl;)
  SparseMatrix<double> M, Minv, LFlatNotMerged, LPreinfNotMerged;
  massmatrix(V, F, igl::MASSMATRIX_TYPE_VORONOI, M);

  if (M.nonZeros() - M.rows() != 0) {
    DEBUG_CMD_MM(
        cout << "error: M.nonZeros()-M.rows()=" << M.nonZeros() - M.rows()
             << " probably a problem in interconnection (non-manifold mesh?)"
             << endl;)
    return false;
  }

  invert_diag(M, Minv);
  cotmatrix(V, F, LFlatNotMerged);

  const int nBnd = bnd.size();
  const int nV = V.rows();
  // sqrt
  VectorXi b(nBnd);
  VectorXd bc(nBnd);
  VectorXd inB(nV);
  VectorXd outZ;
  fora(i, 0, nBnd) b(i) = bnd[i];
  bc.fill(0);

  fora(i, 0, mb.getMeshesCount()) {
    forlist(j, mb.parts1[i]) inB(mb.parts1[i][j]) = -inflationAmount[i];
  }

  SparseMatrix<double> Q = Minv * LFlatNotMerged;
  min_quad_with_fixed(Q, inB, b, bc, SparseMatrix<double>(), VectorXd(), false,
                      outZ);

  fora(i, 0, mb.getMeshesCount()) {
    forlist(j, mb.parts1[i]) {
      int ind = mb.parts1[i][j];
      V(ind, Z_COORD) = sgn(outZ(ind)) * sqrt(abs(outZ(ind)));
    }
  }
  MatrixXd VPreinf = V;
  DEBUG_CMD_MM(cout << "Inflation done" << endl;)

  cotmatrix(VPreinf, F, LPreinfNotMerged);

  I = SparseMatrix<double>(V.rows(), V.rows());

  // merging of interior vertices
  if (armpitsStitching || armpitsStitchingInJointOptimization) {
    mergeArmpitsCorrs.clear();
    forlist(i, selectedMergingCorrs) {
      int p1 = get<0>(mergingPartNum[i]);
      int p2 = get<1>(mergingPartNum[i]);
      int sign = get<2>(mergingPartNum[i]);
      forlist(j, selectedMergingCorrs[i]) {
        auto &el = selectedMergingCorrs[i][j];
        const int bndId = el.second;
        const int meshId = el.first;  // free
        if (j == 0 ||
            j == selectedMergingCorrs[i].size() - 1) {  // first and last
#ifdef ENABLE_MERGING_COMMON_BOUNDARY_IN_PREPROCESS
          // add first and last
          mergeArmpitsCorrs.push_back(make_tuple(bndId, meshId, sign));
          if (!armpitsStitchingInJointOptimization)
            verticesToMerge[bndId].insert(meshId);
#else
          // skip the first and last
#endif

        } else {
          const auto &mc = mb.mergingCorr[p1][p2];
          int counterpartId = -1;
          forlist(k, mc) {
            int i1 = mc[k].second;
            int i2 = mc[k].first;
            if (bndId == i1) {
              counterpartId = i2;
              //          cout << "found counterpart: " << counterpartId <<
              //          endl;
              break;
            }
          }

          if (counterpartId >= 0) {
            //        if (verticesToMergeUsed.find(counterpartId) ==
            //        verticesToMergeUsed.end()) {
            mergeArmpitsCorrs.push_back(
                make_tuple(counterpartId, meshId, sign));
            if (!armpitsStitchingInJointOptimization)
              verticesToMerge[counterpartId].insert(meshId);
            //        }
          } else {
            DEBUG_CMD_MM(cout << "counterpart wasn't found (bndId: " << bndId
                              << ", part1: " << p1 << ", part2: " << p2 << ")"
                              << endl;)
          }
        }
      }
    }
    DEBUG_CMD_MM(cout << "mergeArmpitsCorrs size: " << mergeArmpitsCorrs.size()
                      << endl;)
    if (!armpitsStitchingInJointOptimization) {
#ifdef ENABLE_MERGING_COMMON_BOUNDARY_IN_PREPROCESS
      const int n = 2 * mergeArmpitsCorrs.size();
#else
      const int n = bnd.size() + 2 * mergeArmpitsCorrs.size();
#endif
      b = VectorXi(n);
      VectorXi b2(mergeArmpitsCorrs.size());
      bc = VectorXd(n);
      bc.fill(0);
      VectorXd bc2(mergeArmpitsCorrs.size());
      bc.fill(0);
      Aeq =
          SparseMatrix<double>(bnd.size() + mergeArmpitsCorrs.size(), V.rows());
      Beq = VectorXd(bnd.size() + mergeArmpitsCorrs.size());
      Beq.fill(0);
      forlist(i, mergeArmpitsCorrs) {
        int first = get<0>(mergeArmpitsCorrs[i]);
        int second = get<1>(mergeArmpitsCorrs[i]);
        int sign = get<2>(mergeArmpitsCorrs[i]);
        Aeq.insert(i, first) = sign * 1;    // boundary
        Aeq.insert(i, second) = sign * -1;  // mesh

        b2(i) = b(i) = first;
        b(i + mergeArmpitsCorrs.size()) = second;
        //      double avg = V(first,Z_COORD);
        //      double avg = V(second,Z_COORD);
        double avg = 0.5 * (V(first, Z_COORD) + V(second, Z_COORD));
        //    V(first,Z_COORD) = avg;
        bc2(i) = bc(i) = bc(i + mergeArmpitsCorrs.size()) = avg;
      }
#ifndef ENABLE_MERGING_COMMON_BOUNDARY_IN_PREPROCESS
      if (!performQP) {
        forlist(i, bnd2bnd) {
          int j = i + mergeArmpitsCorrs.size();
          Aeq.insert(j, bnd2bnd[i].first) = 1;
          Aeq.insert(j, bnd2bnd[i].second) = -1;
        }
      }
      fora(i, 0, nBnd) { b(i + 2 * mergeArmpitsCorrs.size()) = bnd[i]; }
#endif

      fora(i, 0, bnd.size()) I.insert(bnd[i], bnd[i]) = 0.00001;
      SparseMatrix<double> Q;
      Q = -(LPreinfNotMerged - I);
      inB = LPreinfNotMerged * V.col(Z_COORD);
      DEBUG_CMD_MM(cout << "Linear solve " << flush;);
      min_quad_with_fixed(Q, inB, VectorXi(), VectorXd(), Aeq, Beq, false,
                          outZ);
      DEBUG_CMD_MM(cout << "done" << endl;)

      V.col(Z_COORD) = outZ;
      VPreinf = V;
    }
  }

#ifndef DISABLE_EQUALITY_VERTICES_MERGING
  DEBUG_CMD_MM(cout << "Merge mesh and remove duplicates ("
                    << verticesToMerge.size() << ") " << flush;)
  vector<bool> removeV;
  vector<int> reindex;
  MeshBuilder::meshMergeAndRemoveDuplicates(V, F, verticesToMerge, Vout, Fout,
                                            mb.parts1, verticesOfParts, removeV,
                                            reindex);
  VPreinfOut = Vout;
  fora(i, 0, V.rows()) if (!removeV[i]) VPreinfOut(reindex[i], Z_COORD) =
      VPreinf(i, Z_COORD);

  cotmatrix(VPreinfOut, Fout, LPreinf);
  DEBUG_CMD_MM(cout << "done" << endl;)
#else
  verticesOfParts = mb.parts1;
  // return the result
  Vout = V;
  Fout = F;
  LPreinf = LPreinfNotMerged;
#endif

  massmatrix(Vout, Fout, igl::MASSMATRIX_TYPE_VORONOI, MFinal);
  invert_diag(MFinal, MinvFinal);
  cotmatrix(Vout, Fout, LFinal);

  MatrixXd VoutFlat = Vout;
  VoutFlat.col(Z_COORD).fill(0);
  cotmatrix(VoutFlat, Fout, LFlat);

#ifndef DISABLE_EQUALITY_VERTICES_MERGING
  // recompute
  const int n = Vout.rows();

  std::vector<std::vector<int>> bnds2 = bnds;
  bnds.clear();
  forlist(i, bnds2) {
    vector<int> bnd;
    forlist(j, bnds2[i]) {
      int ind = bnds2[i][j];
      if (!removeV[ind]) bnd.push_back(reindex[ind]);
    }
    bnds.push_back(bnd);
  }

  vector<int> mergeBnd2 = mergeBnd;
  mergeBnd.clear();
  for (int ind : mergeBnd2) {
    if (!removeV[ind]) mergeBnd.push_back(reindex[ind]);
  }

  Aeq.resize(0, 0);
  Beq.resize(0);
  Aieq.resize(0, 0);
  Bieq.resize(0);
#endif

  return true;
}

bool performReconstruction(RecData &recData, DefData &defData, CPData &cpData,
                           ImgData &imgData) {
  auto &def = defData.def;
  auto &verticesOfParts = defData.verticesOfParts;
  auto &defEng = defData.defEng;
  auto &mesh = defData.mesh;
  auto &defEngMaxIter = defData.defEngMaxIter;
  auto &rigidity = defData.rigidity;

  auto &outlineImgs = imgData.outlineImgs;
  auto &regionImgs = imgData.regionImgs;
  auto &layers = imgData.layers;

  auto &cpsAnim = cpData.cpsAnim;
  auto &savedCPs = cpData.savedCPs;

  auto &triangleOpts = recData.triangleOpts;
  auto &subsFactor = recData.subsFactor;
  auto &armpitsStitching = recData.armpitsStitching;
  auto &armpitsStitchingInJointOptimization =
      recData.armpitsStitchingInJointOptimization;
  auto &smoothFactor = recData.smoothFactor;
  auto &defaultInflationAmount = recData.defaultInflationAmount;
  auto &mergeBothSides = recData.mergeBothSides;
  auto &shiftModelX = recData.shiftModelX;
  auto &shiftModelY = recData.shiftModelY;
  auto &tempSmoothingSteps = recData.tempSmoothingSteps;
  auto &searchThreshold = recData.searchThreshold;
  auto &cpOptimizeForXY = recData.cpOptimizeForXY;
  auto &cpOptimizeForZ = recData.cpOptimizeForZ;
  auto &interiorDepthConditions = recData.interiorDepthConditions;

  if (layers.empty()) {
    return false;
  }

  // 3D reconstruction
  MatrixXd V, VPreinf;
  MatrixXi F;
  const int nLayers = layers.size();

  // subsample the input drawings
  // works for binary images only!
  auto subsample = [](auto &I, int subsampleFactor, int bgColor) -> auto {
    typename remove_reference<decltype(I)>::type out(
        I.w / subsampleFactor, I.h / subsampleFactor, I.ch, I.alphaChannel);
    out.fill(bgColor);
    fora(y, 0, I.h) fora(x, 0, I.w) fora(c, 0, I.ch) {
      const auto &val = I(x, y, c);
      if (val != bgColor)
        out(x / subsampleFactor, y / subsampleFactor, c) = val;
    }
    return out;
  };
  vector<Imguc> regionImgsSubs;
  regionImgsSubs.reserve(nLayers);
  vector<Imguc> outlineImgsSubs;
  outlineImgsSubs.reserve(nLayers);
  int nRegionsToInflate = 0;
  forlist(layerId, layers) {
    const int regId = layers[layerId];
    regionImgsSubs.push_back(
        closeSquare(subsample(regionImgs[regId], subsFactor, 0), Cu{0}, 0));
    outlineImgsSubs.push_back(closeSquare(
        subsample(outlineImgs[regId], subsFactor, 255), Cu{255}, 0));
    nRegionsToInflate++;
  }

  // not used after inflation
  SparseMatrix<double> I, LFlat, LPreinf, LFinal, Aeq, Aieq, MFinal, MinvFinal;
  VectorXd Beq, Bieq;

  vector<tuple<int, int, int>> ineqRegionConds;
  vector<vector<int>> bnds;
  vector<int> mergeBnd;
  vector<tuple<int, int, int>> mergeArmpitsCorrs;
  vector<TriData> triData;
  bool success = true;
  if (nRegionsToInflate > 0) {
    success = reconstruction(
        recData, imgData, outlineImgsSubs, regionImgsSubs, triangleOpts, V, F,
        VPreinf, verticesOfParts, I, LFlat, LPreinf, LFinal, MFinal, MinvFinal,
        Aeq, Beq, Aieq, Bieq, ineqRegionConds, bnds, mergeBnd,
        mergeArmpitsCorrs, triData, smoothFactor, defaultInflationAmount,
        armpitsStitching, armpitsStitchingInJointOptimization, mergeBothSides);
  }

  if (!success) {
    return false;
  }

  fora(i, 0, V.rows()) fora(j, 0, 3) {
    V(i, j) *= subsFactor;
    VPreinf(i, j) *= subsFactor;
  }
  for (auto &el : triData) el.scaling = subsFactor;
  V.col(0).array() += shiftModelX;
  V.col(1).array() += shiftModelY;
  VPreinf.col(0).array() += shiftModelX;
  VPreinf.col(1).array() += shiftModelY;

  // prepare for deformation
  mesh = Mesh3D();
  def = Def3D();
  mesh.createFromMesh(VPreinf, F);

  defData.VCurr = mesh.VCurr;
  defData.Faces = mesh.F;
  defData.VRestOrig = mesh.VRest;

  // load control points and their animations
  cpsAnim.clear();
  istringstream iss(savedCPs);
  loadControlPointsFromStream(iss, cpData, defData, imgData);

  defEng = DefEngARAPL();
  defEng.ineqRegionConds = ineqRegionConds;
  defEng.mergeArmpitsCorrs = mergeArmpitsCorrs;
  defEng.bnds = bnds;
  defEng.partsIds = verticesOfParts;
  defEng.tempSmoothingSteps = tempSmoothingSteps;
  defEng.maxIter = defEngMaxIter;
  defEng.mergeBnd = mergeBnd;
  defEng.searchThreshold = searchThreshold;
  defEng.cpOptimizeForXY = cpOptimizeForXY;
  defEng.cpOptimizeForZ = cpOptimizeForZ;
  defEng.interiorDepthConditions = interiorDepthConditions;
  defEng.armpitsStitchingInJointOptimization =
      armpitsStitchingInJointOptimization;

  defEng.rigidity = rigidity;
  defEng.L = LFinal;
  defEng.M = MFinal;
  defEng.Minv = MinvFinal;
  cotmatrix(VPreinf, F, defEng.L);

  defEng.precompute(def, mesh);

  return true;
}
