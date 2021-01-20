// Copyright (c) 2014-2020 Czech Technical University in Prague
// Licensed under the MIT License.

#include "MeshBuilder.h"
#include <string>
#include <vector>
#include <Eigen/Dense>
#include <Eigen/src/SparseCore/SparseSolverBase-mod.h>
#include <igl/readOBJ.h>
#include <igl/remove_unreferenced.h>
#include <igl/adjacency_matrix.h>
#include <igl/writeOBJ.h>
#include <unordered_map>
#include <tuple>
#include <set>

#ifndef DEBUG_CMD_IR
  #ifdef ENABLE_DEBUG_CMD_IR
    #define DEBUG_CMD_IR(x) {x}
  #else
    #define DEBUG_CMD_IR(x) ;
  #endif
#endif
#ifndef fora
  #define fora(a,s,e) for (int a=s; a<e; a++)
#endif
#ifndef forlist
  #define forlist(a,s) for (size_t a=0; a<(s).size(); a++)
#endif
#ifndef eequal
  #define eequal(a,b) (fabs((a)-(b))) < 0.0001
#endif

using namespace std;
using namespace Eigen;
using namespace igl;

MeshBuilder::MeshBuilder()
  : counter(0), Z_COORD(2), scaling(1), spacing(0.5), pits(0)
{
}

MeshBuilder::MeshBuilder(int Z_COORD, double spacing, double scaling)
  : counter(0), Z_COORD(Z_COORD), scaling(scaling), spacing(spacing), pits(0)
{
}

void MeshBuilder::parseAnnotations(int ann, vector<int> &annParsed)
{
  annParsed.resize(20);
  fora(i, 0, 20) { annParsed[i] = ann % 10; ann /= 10; }
}

void MeshBuilder::processTwoSidedMesh(MatrixXd &Vin1, MatrixXi &Fin1, MatrixXd &Vin2, MatrixXi &Fin2, const bool computeBnds, std::vector<std::vector<int>> &customBnds, const bool removeUnreferenced)
{
  processPlanarMesh(Vin1, Fin1, computeBnds, customBnds, removeUnreferenced);
  processPlanarMesh(Vin2, Fin2, computeBnds, customBnds, removeUnreferenced);

  MatrixXd &V2 = Vs[counter-1];
  MatrixXi &F2 = Fs[counter-1];
  MatrixXd V1 = V2;
  MatrixXi F1 = F2;
  vector<int> &bnd = bnds[counter-1];
  vector<vector<int>> &annotations = allAnnotations[counter-1];
  vector<int> &partComponents = this->partComponents[counter-1];
  vector<bool> isBndForMerging(V1.rows(), false);
  int nRemove = 0;

  // mark boundary vertices that are not free (neumann) ones
  vector<vector<int>> partBnd;
  vector<int> starts;
  getBndAndStart(counter-1, partBnd, starts);
  forlist(b, partBnd) {
    const vector<int> &pbnd = partBnd[b];
    int start = starts[b];
    fora(i, start, pbnd.size() + start) {
      int j = i % pbnd.size();
      const vector<int> &annI = annotations[pbnd[j]]; // current
      const vector<int> &annIn = annotations[pbnd[(i+1)%pbnd.size()]]; // next
      const vector<int> &annIp = i>0 ? annotations[pbnd[(i-1)%pbnd.size()]] : annotations[pbnd[pbnd.size()-1]]; // previous
      bool neumann = false, neumannFirstLast = false;
      if (annI[2] && (!annIn[2] || !annIp[2])) { neumannFirstLast = true; }
      if (annI[2]) neumann = true;
      if (!neumann || neumannFirstLast) { // neumann
        isBndForMerging[pbnd[j]] = true;
        nRemove++;
      }
    }
  }

  // recreate vertices, annotations
  vector<int> reindex(V1.rows());
  const int newSize = V1.rows()-nRemove;
  V2.resize(newSize,3);
  vector<vector<int>> newAnnotations(newSize);
  int k = 0;
  fora(j, 0, V1.rows()) {
    if (isBndForMerging[j]) {
      reindex[j] = -Vs[counter-2].rows()+j;
      continue;
    }
    V2.row(k) = V1.row(j);
    reindex[j] = k;
    newAnnotations[k] = annotations[j];
    k++;
  }
  annotations = newAnnotations;

  // reindex faces
  fora(j, 0, F1.rows()) {
    fora(k, 0, F1.cols()) {
      F2(j,k) = reindex[F1(j,k)];
    }
  }

  // recreate bnds
  vector<int> newPartComponents(bnd.size()-nRemove,0);
  vector<int> newBnd(bnd.size()-nRemove);
  k = 0;
  forlist(b, partBnd) {
    const vector<int> &pbnd = partBnd[b];
    int start = starts[b];
    fora(i, start, pbnd.size() + start) {
      int j = i % pbnd.size();
      if (isBndForMerging[pbnd[j]]) continue;
      newBnd[k] = reindex[pbnd[j]];
      newPartComponents[k] = partComponents[j];
      k++;
    }
  }
//  forlist(j, bnd) {
//    if (isFixedBnd[bnd[j]]) continue;
//    newBnd[k] = reindex[bnd[j]];
//    newPartComponents[k] = partComponents[j];
//    k++;
//  }
  bnd = newBnd;
  partComponents = newPartComponents;

  DEBUG_CMD_IR(cout << "Merged boundary vertices: back side num verts: " << V2.rows() << ", back side num bnds: " << bnd.size() << endl;);
}

void MeshBuilder::processPlanarMesh(MatrixXd &Vin, MatrixXi &Fin, const bool computeBnds, std::vector<std::vector<int>> &customBnds, const bool removeUnreferenced)
{
  Vs.resize(counter+1); Fs.resize(counter+1);
  bnds.resize(counter+1);
  neumannBnds.resize(counter+1);
  mergeBnds.resize(counter+1);
  allAnnotations.resize(counter+1);
  mergeVs.resize(counter+1); mergeFs.resize(counter+1);
  bconds_all.resize(counter+1);
  parts1.resize(counter+1);
  graftingCorr.resize(counter+1);
  partComponents.resize(counter+1);

  if (Vin.size() == 0) {
    counter++;
    return;
  }

  // remove vertices that don't belong to any face
  MatrixXd Vin_repaired; MatrixXi Fin_repaired, I;
  if (removeUnreferenced) igl::remove_unreferenced(Vin, Fin, Vin_repaired, Fin_repaired, I);
  else {
    Vin_repaired = Vin;
    Fin_repaired = Fin;
  }

  MatrixXd &V = Vs[counter] = Vin_repaired;
  MatrixXi &F = Fs[counter] = Fin_repaired;
  vector<int> &bnd = bnds[counter];
  vector<vector<int>> &annotations = allAnnotations[counter];
  vector<vector<int>> &partComponents = this->partComponents;

  VectorXi b;
  annotations.resize(V.rows());
  fora(i, 0, V.rows()) {
    parseAnnotations((int)(V(i,Z_COORD)), annotations[i]);
    V(i,Z_COORD) = 0;
  }

  if (computeBnds) {
    assert("multiple_boundary_loop not implemented" && false);
//    multiple_boundary_loop(V, F, b, partComponents[counter]);
    bnd.clear();
    fora(i, 0, b.size()) bnd.push_back(b(i));
  } else {
    bnd.clear();
    forlist(i, customBnds) {
      forlist(j, customBnds[i]) {
        bnd.push_back(customBnds[i][j]);
        partComponents[counter].push_back(i);
      }
    }
  }

  F.col(1).swap(F.col(2));

  DEBUG_CMD_IR(printf("[%d]     V: %6ld, F: %6ld, bnd: %6ld, ann: %6ld\n", counter, V.rows(), F.rows(), bnd.size(), annotations.size()););

  counter++;
}

#if 0
void MeshBuilder::loadPlanarMesh(string nameOfPartWithoutSuffix, string suffix)
{
  MatrixXd V; MatrixXi F;
  igl::readOBJ(nameOfPartWithoutSuffix + suffix, V, F);

  processPlanarMesh(V, F);
  filenames.push_back(nameOfPartWithoutSuffix + suffix);
}
#endif

void MeshBuilder::getMesh(int num, MatrixXd &V, MatrixXi &F)
{
  V = Vs[num];
  F = Fs[num];
}

void MeshBuilder::getBoundary(int num, vector<int> &bnd, vector<int> &neumannBnd)
{
  bnd = bnds[num];
}

// get<0>(candidates) is index, get<1>(candidates) are vertex coordinates, get<2>(V) is custom number for a vertex
// get<0>(corr) is index of vertex anywhere (inside/boundary) on the mesh, get<1>(corr) is index of candidate vertex (typically the boundary), get<3>(V) is custom number for a vertex
void MeshBuilder::findCorrespondences(const vector<V2VCorrCandidate> &candidates, const MatrixXd &mesh, vector<V2VCorr> &corr, int maxindex, const vector<int> &part, const vector<bool> &isBnd)
{
  if (candidates.size() == 0) return;

  vector<bool> used(maxindex, false);

  int X_COORD = (Z_COORD+1)%3, Y_COORD = (Z_COORD+2)%3;
  bool subsequent = true;
  int i = 0;
  fora(k, -1, (int)candidates.size()) {
    bool found = false;
    if (k == -1) i = candidates.size()-1; else i = k;

    const int candidateVInd = candidates[i].ind;
    const Vector3d &candidateVCoord = candidates[i].coord;
    for (int j2=part.size()-1; j2>=0; j2--) {
      int j = part[j2];
      if (eequal(candidateVCoord(X_COORD), mesh(j,X_COORD)) &&
          eequal(candidateVCoord(Y_COORD), mesh(j,Y_COORD))) {
        // if there are multiple correspondences, record only the first one
        if (maxindex == 0 || !used[candidateVInd]) {
          if (k >= 0) {
            corr.push_back(V2VCorr{candidates[i].type, (isBnd[j]?VertexType::bnd:VertexType::in), candidateVInd, j, candidates[i].customNum, subsequent});
//             if (!subsequent) printf("notsubs: %d\n",i);
            if (maxindex > 0) used[candidateVInd] = true;
          }
          found = true;
          break;
        }
      }
    }
    subsequent = found;
  }
}

static double atan2_mod(double y, double x)
{
  double angle = atan2(y,x);
  return angle<0 ? 2*M_PI+angle : angle;
}

static double get_angle(double x1, double y1, double x2, double y2)
{
  double angle = atan2_mod(x1*y2-x2*y1, x1*x2+y1*y2);
  return angle;
}

void MeshBuilder::mergeMesh(const std::vector<std::pair<int,int>> &corr, bool reverseN)
{
  if (corr.empty()) return;
  const int X_COORD = (Z_COORD+1)%3, Y_COORD = (Z_COORD+2)%3;

  // find incident faces for grafting boundary
  vector<vector<int>> incidentFaces(corr.size());
  fora(i, 0, corr.size()) fora(j, 0, Fc.rows()) fora(k, 0, 3) {
    if (Fc(j,k) == corr[i].first) incidentFaces[i].push_back(j);
  }

  // reconnect mesh topology
  // it's not possible to also merge first and last vertices, the final mesh would be non-manifold
  fora(i, 1, corr.size()-1) {
    Vector3d A, B, C;
    double alphaMax;
    C = Vc.row(corr[i].second);
    A = (Vector3d)Vc.row(corr[i-1].second) - C;
    B = (Vector3d)Vc.row(corr[i+1].second) - C;
    alphaMax = get_angle(A(X_COORD), A(Y_COORD), B(X_COORD), B(Y_COORD));
    fora(j, 0, incidentFaces[i].size()) {
      int k = incidentFaces[i][j];
      int l = 0;
      fora(m, 0, 3) if (Fc(k,m) == corr[i].first) l = m;
      int n = (reverseN ? 2 : 1);
      B = (Vector3d)Vc.row(Fc(k,(l+n)%3)) - C;
      double alpha = get_angle(A(X_COORD), A(Y_COORD), B(X_COORD), B(Y_COORD));
      if (alpha <= alphaMax && alpha > 0) {
        const int bndId = corr[i].second;
        const int meshId = corr[i].first;
        Fc(k,l) = bndId;
      }
    }
  }
}

void MeshBuilder::getBndAndStart(int partId, std::vector<std::vector<int>> &partBnd, std::vector<int> &starts)
{
  // recreate the boundary for each separate component - TODO rework and maybe move somewhere else
  vector<int> &bnd = bnds[partId];
  vector<vector<int>> &annotations = allAnnotations[partId];

  int numParts = 0;
  forlist(i, partComponents[partId]) numParts = partComponents[partId][i]>numParts ? partComponents[partId][i] : numParts;
  partBnd.resize(numParts+1);
  forlist(i, partComponents[partId]) partBnd[partComponents[partId][i]].push_back(bnd[i]);

  // analyze annotations for each boundary
  starts.resize(numParts+1);
  forlist(b, partBnd) {
    const vector<int> &pbnd = partBnd[b];
    // We must avoid starting the loop in merging boundary, so we first find a suitable position for start.
    // This does not need to hold anymore: We assume that there is a non-merging vertex in the boundary.
    int start = -1;
    fora(i, 0, pbnd.size()) {
      int j = i%pbnd.size();
      const vector<int> &annI = annotations[pbnd[j]];
      if (!annI[3]) { // annI[3] is merging boundary
        start = j;
        break;
      }
    }
    if (start == -1) start = 0;
//      if (start == -1) {
//        // No non-merging vertex in the boundary, i.e. the whole boundary should to be merged
//        forlist(i, pbnd) {
//          mergeBnd.push_back(pbnd[i]);
//          tmpbnd.push_back(pbnd[i]);
//        }
//      } else
    starts[b] = start;
  }
}

void append_obj(MatrixXd &V, MatrixXi &F, MatrixXd &outV, MatrixXi &outF, vector<int> &bnd, vector<int> &neumannBnd, vector<int> &outbnd, vector<int> &outNeumannBnd, vector<int> &classes, vector<vector<int>> &allVertices)
{
  MatrixXi F2 = F; F2.array() += outV.rows();

  fora(i, 0, neumannBnd.size()) outNeumannBnd.push_back(neumannBnd[i] + outV.rows());
  fora(i, 0, bnd.size()) outbnd.push_back(bnd[i] + outV.rows());
  int newClass = (classes.size() > 0 ? classes[classes.size()-1] : -1) + 1;
  fora(i, 0, V.rows()) classes.push_back(newClass);

  vector<int> indices(V.rows());
  fora(i, 0, V.rows()) indices[i] = i + outV.rows();
  allVertices.push_back(indices);

  if (outV.rows() > 0) {
    MatrixXd tmpV(outV.rows() + V.rows(), V.cols());
    MatrixXi tmpF(outF.rows() + F.rows(), F.cols());
    tmpV << outV, V;
    outV = tmpV;
    tmpF << outF, F2;
    outF = tmpF;
  } else {
    outV = V;
    outF = F;
  }
}

void MeshBuilder::findBndCandidates(std::vector<std::vector<V2VCorrCandidate>> &ineqCandidates,
                                    std::vector<std::vector<V2VCorrCandidate>> &eqCandidates,
                                    std::vector<std::vector<V2VCorrCandidate>> &graftingBndCandidates)
{
  fora(a, 0, counter) {
    MatrixXd &V = Vs[a];
    vector<VectorXd> tmpV;
    MatrixXi &F = Fs[a];
    vector<int> &bnd = bnds[a], neumannBnd = neumannBnds[a];
    vector<int> &mergeBnd = mergeBnds[a];
    vector<vector<int>> &partComponents = this->partComponents;
    vector<vector<int>> &annotations = allAnnotations[a];
    vector<int> tmpbnd, tmpNeumannBnd;

    // analyze annotations for each boundary
    vector<vector<int>> partBnd;
    vector<int> starts;
    getBndAndStart(a, partBnd, starts);
    forlist(b, partBnd) {
      const vector<int> &pbnd = partBnd[b];
      int start = starts[b];
      fora(i, start, pbnd.size() + start) {
        int j = i % pbnd.size();
        const vector<int> &annI = annotations[pbnd[j]]; // current
        const vector<int> &annIn = annotations[pbnd[(i+1)%pbnd.size()]]; // next
        const vector<int> &annIp = i>0 ? annotations[pbnd[(i-1)%pbnd.size()]] : annotations[pbnd[pbnd.size()-1]]; // previous

        bool neumannIn = false, neumannFirstLast = false, mergeIn = false, mergeFirstLast = false;
        if (annI[2] && annIn[2] && annIp[2]) { neumannIn = true; }
        if (annI[3] && annIn[3] && annIp[3]) { mergeIn = true; }
        if (annI[2] && (!annIn[2] || !annIp[2])) { neumannFirstLast = true; }
        if (annI[3] && (!annIn[3] || !annIp[3])) { mergeFirstLast = true; }
        int customNum = annI[4];

        if (mergeIn || mergeFirstLast) mergeBnd.push_back(pbnd[j]);
//        if (!((neumannIn && boundaryConditions.empty()) || (neumannIn && !boundaryConditions.empty() && boundaryConditions[a][0].compare("neumann") == 0))) tmpbnd.push_back(pbnd[j]);
        if (!neumannIn) tmpbnd.push_back(pbnd[j]);
        else tmpNeumannBnd.push_back(pbnd[j]);

        bool shifting = true;
//        if ((mergeIn && boundaryConditions.empty()) || (mergeIn && !boundaryConditions.empty() && boundaryConditions[a][0].compare("merge") == 0)) shifting = false;
        if (mergeIn) shifting = false;

        if (shifting || graftingBndStitching) {
          // ann[0]=1 or ann[1]=1 means that equalities/inequalities are disabled for affected vertices
          if (!annI[1]) ineqCandidates[a].push_back(V2VCorrCandidate{VertexType::bnd, (int)(pbnd[j] + Vc.rows()), V.row(pbnd[j]), customNum});
          if (!annI[0]) eqCandidates[a].push_back(V2VCorrCandidate{VertexType::bnd, (int)(pbnd[j] + Vc.rows()), V.row(pbnd[j]), customNum});
        }
      }
    }

    bnd = tmpbnd;
    neumannBnd = tmpNeumannBnd;

    // also find inner vertices candidates
    vector<bool> isBnd(V.rows(), false);
    forlist(i, bnd) isBnd[bnd[i]] = true;
    for(int ind : neumannBnd) isBnd[ind] = true;
    fora(i, 0, V.rows()) {
      if (isBnd[i]) continue; // go through only non-boundary vertices
      const int customNum = 0; // TODO: currently this cannot be specified in UI
      eqCandidates[a].push_back(V2VCorrCandidate{VertexType::in, (int)(i + Vc.rows()), V.row(i), customNum});
      ineqCandidates[a].push_back(V2VCorrCandidate{VertexType::in, (int)(i + Vc.rows()), V.row(i), customNum});
    }

    if (flipReg[a]) fora(i, 0, F.rows()) std::swap(F(i,1), F(i,2));

    MatrixXd oldV = V, newV;
    MatrixXi oldF = F, newF;
    vector<int> tmp_part;

    newV = V; newF = F;
    fora(i, 0, V.rows()) tmp_part.push_back(i);

    // here we assume that mergeBnd contains vertices that will be merged
    forlist(i, mergeBnd) {
      graftingBndCandidates[a].push_back(V2VCorrCandidate{VertexType::bnd, (int)(mergeBnd[i] + Vc.rows()), newV.row(mergeBnd[i]), 0}); // TODO: change 0 to customNum!!!!
//      b_eq_add1[a].push_back(make_pair(mergeBnd[i] + Vc.rows(), newV.row(mergeBnd[i])));
    }

    forlist(i, tmp_part) {
      parts1[a].push_back(tmp_part[i] + Vc.rows());
      partsc1.push_back(tmp_part[i] + Vc.rows());
    }

    for(int &i : bnd) i += Vc.rows();
    for(int &i : neumannBnd) i += Vc.rows();

    vector<int> classes;
    vector<vector<int>> allVertices;
    if (newV.rows() > 0) append_obj(newV, newF, Vc, Fc, tmpbnd, tmpNeumannBnd, bndc, neumannBndc, classes, allVertices);
  }
}

void MeshBuilder::createCompleteMesh()
{
  vector<vector<V2VCorrCandidate>> ineqCandidates(counter), eqCandidates(counter), graftingBndCandidates(counter);
  findBndCandidates(ineqCandidates, eqCandidates, graftingBndCandidates);

  vector<bool> isBnd(Vc.rows(), false);
  forlist(i, bndc) isBnd[bndc[i]] = true;
  for(int ind : neumannBndc) isBnd[ind] = true;

  mergingCorr = vector<vector<vector<pair<int,int>>>>(counter, vector<vector<pair<int,int>>>(counter));
  eqCorr = ineqCorr = vector<vector<vector<V2VCorr>>>(counter, vector<vector<V2VCorr>>(counter));
  vector<V2VCorr> corr2;
  fora(a, 0, counter) {
    fora(b, 0, counter) {
      if (a == b) continue;
      DEBUG_CMD_IR(printf("[%d,%d] ", a, b););
      corr2.clear();
      vector<int> parts(parts1[b].size());
      forlist(i, parts1[b]) parts[i] = parts1[b][i];
      findCorrespondences(eqCandidates[a], Vc, corr2, Vc.rows(), parts, isBnd);
      fora(i, 0, corr2.size()) eqCorr[a][b].push_back(corr2[i]);
      DEBUG_CMD_IR(printf("bcondsEq: %6ld, ", corr2.size()););
      corr2.clear();
      findCorrespondences(ineqCandidates[a], Vc, corr2, Vc.rows(), parts, isBnd);
      fora(i, 0, corr2.size()) ineqCorr[a][b].push_back(corr2[i]);
      DEBUG_CMD_IR(printf("bcondsIneq: %6ld, ", corr2.size()););
      corr2.clear();
      findCorrespondences(graftingBndCandidates[a], Vc, corr2, Vc.rows(), parts, isBnd);
      fora(i, 0, corr2.size()) mergingCorr[a][b].push_back(make_pair(corr2[i].b, corr2[i].a));
      DEBUG_CMD_IR(printf("merge: %6ld/%6ld (candidates/corrs)", graftingBndCandidates[a].size(), corr2.size()););
      DEBUG_CMD_IR(printf("\n"););
    }
  }
}

void MeshBuilder::mirrorMesh(const MatrixXd &inV, const MatrixXi &inF, const MatrixXd &inV2, const MatrixXi &inF2, const std::vector<int> &bnd, const std::vector<int> &neumannBnd, MatrixXd &outV, MatrixXi &outF, vector<int> &part1, vector<int> &part2, vector<int> &part2NeumannBnd)
{
  assert(addressof(inV) != addressof(outV) && addressof(inF) != addressof(outF));
  assert(addressof(inV2) != addressof(outV) && addressof(inF2) != addressof(outF));

  const int nBnd = bnd.size();
  const int nInV1 = inV.rows();
  const int nInV2 = inV2.rows();
  vector<bool> isBnd(nInV2, false); // we assume that boundary vertices are stored at the beginning of list of mesh vertices and that boundary vertices are the same for inV1 and inV2
  fora(i, 0, nBnd) isBnd[bnd[i]] = true;
  vector<int> reindex(nInV2);

  // copy all vertices of the original mesh and add all non-boundary vertices of the mirrored mesh
  outV.resize(nInV1 + nInV2 - nBnd, inV.cols());
  fora(i, 0, nInV1) outV.row(i) = inV.row(i);
  int j = 0;
  fora(i, 0, nInV2) {
    if (!isBnd[i]) {
      const int ind = j + nInV1;
      outV.row(ind) = inV2.row(i);
      reindex[i] = ind; // index of mirrored vertex i
      j++;
    } else {
      reindex[i] = i;
    }
  }

  // create Neumann boundary for mirrored part while keeping correct ordering of vertices (out of the grafting boundary)
  part2NeumannBnd.resize(neumannBnd.size());
  forlist(i, neumannBnd) part2NeumannBnd[i] = reindex[neumannBnd[i]];

  // store which part of the mesh is the original (part1) and which is mirrored (part2)
  part1.resize(nInV1);
  part2.resize(j);
  fora(i, 0, nInV1) part1[i] = i;
  fora(i, 0, j) part2[i] = i + nInV1;

  // copy all faces of the original mesh and add all faces of the mirrored mesh while also reindexing their vertices
  const int nInF1 = inF.rows();
  const int nInF2 = inF2.rows();
  outF.resize(nInF1 + nInF2, inF.cols());
  fora(i, 0, nInF1) {
    fora(j, 0, inF.cols()) {
      const int Vind = inF(i,j);
      outF(i,j) = Vind;
    }
  }
  fora(i, 0, nInF2) {
    fora(j, 0, inF2.cols()) {
      const int Vind = inF2(i,j);
      outF(i + nInF1,j) = reindex[Vind];
    }
    std::swap(outF(i+nInF1,1), outF(i+nInF1,2)); // flip normal
  }
}

void MeshBuilder::getCompleteMesh(MatrixXd &V, MatrixXi &F)
{
  if (Vc.rows() == 0) createCompleteMesh();
  V = Vc;
  F = Fc;
  DEBUG_CMD_IR(printf("total vertices: %ld, total faces: %ld\n", V.rows(), F.rows()););
}

void MeshBuilder::getCompleteBoundary(vector<int> &bnd, vector<int> &neumannBnd)
{
  if (Vc.rows() == 0) createCompleteMesh();
  bnd = bndc;
  neumannBnd = neumannBndc;
  DEBUG_CMD_IR(printf("total bnd: %ld, total neumann bnd: %ld\n", bnd.size(), neumannBnd.size()););
}

void MeshBuilder::getCompleteBoundary(vector<int> &bnd)
{
  if (Vc.rows() == 0) createCompleteMesh();
  bnd = bndc;
  DEBUG_CMD_IR(printf("total bnd: %ld\n", bnd.size()););
}

void MeshBuilder::getClasses(vector<int> &classes)
{
  if (Vc.rows() == 0) createCompleteMesh();
  classes = this->classes;
}

void MeshBuilder::getAnnotationsForVertices(std::vector<std::vector<int>> &annotations)
{
  annotations.clear();
  fora(i, 0, counter) fora(j, 0, allAnnotations[i].size()) {
    annotations.push_back(allAnnotations[i][j]);
  }
  DEBUG_CMD_IR(printf("size:%ld\n", annotations.size()););
}

int MeshBuilder::getMeshesCount()
{
  return counter;
}

void MeshBuilder::meshMergeAndRemoveDuplicates(const Eigen::MatrixXd &inV, const Eigen::MatrixXi &inF, const std::vector<std::pair<int,int>> &verticesToMerge, Eigen::MatrixXd &outV, Eigen::MatrixXi &outF, std::vector<std::vector<int>> &inVerticesOfParts, std::vector<std::vector<int>> &outVerticesOfParts)
{
  const int nV = inV.rows();
  vector<bool> removeV(nV, false);
  vector<int> reindex(nV, -1);
  meshMergeAndRemoveDuplicates(inV, inF, verticesToMerge, outV, outF, inVerticesOfParts, outVerticesOfParts, removeV, reindex);
}

void MeshBuilder::meshMergeAndRemoveDuplicates(const Eigen::MatrixXd &inV, const Eigen::MatrixXi &inF, const std::vector<std::pair<int,int>> &verticesToMerge, Eigen::MatrixXd &outV, Eigen::MatrixXi &outF, std::vector<std::vector<int>> &inVerticesOfParts, std::vector<std::vector<int>> &outVerticesOfParts, std::vector<bool> &removeV, std::vector<int> &reindex)
{
  unordered_map<int,vector<int>> verticesToMergeMap;
  for(auto &el : verticesToMerge) {
    verticesToMergeMap[el.first].push_back(el.second);
  }
  meshMergeAndRemoveDuplicates(inV, inF, verticesToMergeMap, outV, outF, inVerticesOfParts, outVerticesOfParts, removeV, reindex);
}

void MeshBuilder::meshMergeAndRemoveDuplicates(const Eigen::MatrixXd &inV, const Eigen::MatrixXi &inF, const std::unordered_map<int,std::set<int>> &verticesToMerge, Eigen::MatrixXd &outV, Eigen::MatrixXi &outF, std::vector<std::vector<int>> &inVerticesOfParts, std::vector<std::vector<int>> &outVerticesOfParts, std::vector<bool> &removeV, std::vector<int> &reindex)
{
  unordered_map<int,vector<int>> verticesToMergeMap;
  for(auto &el : verticesToMerge) {
    for(auto &el2 : el.second) {
      verticesToMergeMap[el.first].push_back(el2);
    }
  }
  meshMergeAndRemoveDuplicates(inV, inF, verticesToMergeMap, outV, outF, inVerticesOfParts, outVerticesOfParts, removeV, reindex);
}

/**
 * verticesToMergeMap: first: index of vertex that will remain, second: vertices to be merged with first and then removed
 */
void MeshBuilder::meshMergeAndRemoveDuplicates(const Eigen::MatrixXd &inV, const Eigen::MatrixXi &inF, const std::unordered_map<int,std::vector<int>> &verticesToMerge, Eigen::MatrixXd &outV, Eigen::MatrixXi &outF, std::vector<std::vector<int>> &inVerticesOfParts, std::vector<std::vector<int>> &outVerticesOfParts, std::vector<bool> &removeV, std::vector<int> &reindex)
{
  // find incident faces for all vertices
  const int nF = inF.rows();
  const int nV = inV.rows();

  removeV.resize(nV, false);
  reindex.resize(nV, -1);

  vector<vector<pair<int,int>>> incidentFaces(nV);
  fora(i, 0, nF) fora(j, 0, 3) incidentFaces[inF(i,j)].push_back(make_pair(i,j));

  // mark unused vertices for removal
  int nRemoved = 0;
  for(auto &el : verticesToMerge) {
    for(auto &indV : el.second) {
      removeV[indV] = true;
      nRemoved++;
    }
  }
  const int nNew = nV-nRemoved;
  outV.resize(nNew,3);
  int j = 0;
  fora(i, 0, nV) {
    if (!removeV[i]) {
      reindex[i] = j;
      outV.row(j) = inV.row(i);
      j++;
    }
  }

  // copy faces
  outF.resize(nF,inF.cols());
  fora(i, 0, nF) fora(j, 0, outF.cols()) outF(i,j) = reindex[inF(i,j)];
  // merge faces
  for(auto &el : verticesToMerge) {
    const int indVRemain = el.first;
    for(const int indVRemove : el.second) {
      // reconnect
      forlist(j, incidentFaces[indVRemove]) {
        const pair<int,int> &indF = incidentFaces[indVRemove][j];
        outF(indF.first,indF.second) = reindex[indVRemain];
      }
    }
  }

  // update verticesOfParts
  outVerticesOfParts.clear();
  forlist(i, inVerticesOfParts) {
    vector<int> verts;
    forlist(j, inVerticesOfParts[i]) {
      int ind = inVerticesOfParts[i][j];
      if (!removeV[ind]) verts.push_back(reindex[ind]);
    }
    outVerticesOfParts.push_back(verts);
  }
}
