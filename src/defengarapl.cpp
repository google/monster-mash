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

#include "defengarapl.h"

#include <igl/arap_linear_block.h>
#include <igl/cotmatrix.h>
#include <igl/fit_rotations.h>
#include <igl/invert_diag.h>
#include <igl/massmatrix.h>
#include <image/image.h>

#include <set>

#include "macros.h"

using namespace Eigen;
using namespace igl;
using namespace std;

DefEngARAPL::DefEngARAPL() { rigidity = 0.999; }

DefEngARAPL::~DefEngARAPL() {}

bool DefEngARAPL::checkData(const Eigen::MatrixXd &VCurr,
                            const Eigen::MatrixXd &VRest,
                            const Eigen::MatrixXi &F) {
  if (VCurr.size() == 0 || VRest.size() == 0 || F.size() == 0) {
    DEBUG_CMD_MM(cerr << "DefEngARAPL: mesh is empty, terminating deformation"
                      << endl;);
    return false;
  }
  return true;
}

bool DefEngARAPL::checkData(const Mesh3D &mesh) {
  return checkData(mesh.VCurr, mesh.VRest, mesh.F);
}

void DefEngARAPL::prepare(const Eigen::MatrixXd &V, const Eigen::MatrixXi &F) {
  SparseMatrix<double> K0, K1, K2;
  ARAPEnergyType type = ARAP_ENERGY_TYPE_SPOKES_AND_RIMS;
  arap_linear_block(V, F, 0, type, K0);
  arap_linear_block(V, F, 1, type, K1);
  arap_linear_block(V, F, 2, type, K2);
  K = cat(2, cat(2, K0, K1), K2);

  const int n = V.rows();
  SparseMatrix<double> Z(n, n);
  SparseMatrix<double> ZZ(n, n * 2);
  CSM = cat(1, cat(1, cat(2, K0, ZZ), cat(2, cat(2, Z, K1), Z)), cat(2, ZZ, K2))
            .transpose();
}

void DefEngARAPL::solveARAP(igl::min_quad_with_fixed_data<double> &data,
                            const Eigen::SparseMatrix<double> &lambda,
                            const Eigen::SparseMatrix<double> &lambdaInv,
                            const Eigen::SparseMatrix<double> &K,
                            const Eigen::MatrixXd &VRest,
                            const Eigen::VectorXd &Beq, Eigen::MatrixXd &VCurr,
                            Eigen::MatrixXd &sol) {
  const int n = VRest.rows();

  MatrixXd S = CSM * VCurr.replicate(3, 1);
  S /= S.array().abs().maxCoeff();

  MatrixXd Rtmp;
  fit_rotations_planar(S, Rtmp);
  Rtmp.transposeInPlace();

  MatrixXd R(3 * n, 3);
  fora(i, 0, n) {
    R.row(i) = Rtmp.row(3 * i);
    R.row(n + i) = Rtmp.row(3 * i + 1);
    R.row(2 * n + i) = Rtmp.row(3 * i + 2);
  }

  MatrixXd B;
  B = -K * R;
  B = (lambdaInv * B + lambda * VCurr);

  sol.resize(data.n + Beq.rows(), VCurr.cols());

  VectorXd out(VCurr.rows()), solC;
  fora(i, 0, VCurr.cols()) {
    VectorXd BeqC;
    min_quad_with_fixed_solve(data, B.col(i), VectorXd(), Beq, out, solC);
    VCurr.col(i) = out;
    sol.col(i) = solC;
  }
}

template <typename T, typename ShaderT>
static void rasterizeSimple(const Eigen::Matrix<T, -1, -1> &V,
                            const Eigen::MatrixXi &F, int width, int height,
                            ShaderT &shader) {
  auto edgeFn2D = [](const auto &a, const auto &b,
                     const Vector2d &p) -> double {
    return (p(0) - a(0)) * (b(1) - a(1)) - (p(1) - a(1)) * (b(0) - a(0));
  };

  fora(i, 0, F.rows()) {
    const Vector3d &v0 = V.row(F(i, 0));
    const Vector3d &v1 = V.row(F(i, 1));
    const Vector3d &v2 = V.row(F(i, 2));
    const int xMin =
        max(0, static_cast<int>(floor(min(v0(0), min(v1(0), v2(0))))));
    const int xMax =
        min(width - 1, static_cast<int>(ceil(max(v0(0), max(v1(0), v2(0))))));
    const int yMin =
        max(0, static_cast<int>(floor(min(v0(1), min(v1(1), v2(1))))));
    const int yMax =
        min(height - 1, static_cast<int>(ceil(max(v0(1), max(v1(1), v2(1))))));
    fora(y, yMin, yMax) fora(x, xMin, xMax) {
      const Vector2d p(x, y);
      const double w0 = edgeFn2D(v0, v1, p);
      const double w1 = edgeFn2D(v1, v2, p);
      const double w2 = edgeFn2D(v2, v0, p);
      shader(x, y, i, F(i, 0), F(i, 1), F(i, 2), w0, w1, w2);
    }
  }
}

void DefEngARAPL::prepareActiveSetEqs(const Eigen::MatrixXd &VCurr,
                                      const Eigen::MatrixXi &FCurr) {
  // create corresponding points
  vector<tuple<int, int, int>> ineqCorrs;
  vector<bool> isBnd(VCurr.rows(), false);
  forlist(i, bnds) forlist(j, bnds[i]) isBnd[bnds[i][j]] = true;

  vector<bool> isMergeBnd(VCurr.rows(), false);
  if (interiorDepthConditions) {
    for (int ind : mergeBnd) isMergeBnd[ind] = true;
  }

  vector<bool> used(VCurr.rows(), false);
  if (armpitsStitchingInJointOptimization) {
    for (auto &el : mergeArmpitsCorrs) {
      const int a = get<0>(el);
      const int b = get<1>(el);
      used[a] = true;
      used[b] = true;
    }
  }
  vector<bool> faceUsed(FCurr.rows(), false);

  vector<int> verticesToParts(VCurr.rows());
  const int nParts = partsIds.size();
  forlist(i, partsIds) forlist(j, partsIds[i]) verticesToParts[partsIds[i][j]] =
      i;

  // rasterize face IDs for each part (rasterizes both front and back facing
  // triangles)
  Img<int> frameBuffer(1600, 800, nParts);
  frameBuffer.fill(-1);
  auto shader = [&](int x, int y, int faceId, int v0, int v1, int v2, double w0,
                    double w1, double w2) {
    //    if ((w0 < 0 && w1 < 0 && w2 < 0) || // back facing
    //        (w0 >= 0 && w1 >= 0 && w2 >= 0)) // front facing
    //    {
    const int partId = verticesToParts[v0];
    frameBuffer(x, y, partId) = faceId;
    //    }
  };
  const Matrix4d M = Matrix4d::Identity();
  rasterizeSimple<double>(VCurr, FCurr, frameBuffer.w, frameBuffer.h, shader);

  for (const auto &el : ineqRegionConds) {
    const int regionIdBnd = get<0>(el);
    const int regionIdMesh = get<1>(el);
    const int sign = get<2>(el);

    // find existing correspondences for regions boundary
    vector<int> *vertexIds = &bnds[regionIdBnd];
    if (interiorDepthConditions) vertexIds = &partsIds[regionIdBnd];

    for (const int bndId : *vertexIds) {
      if (interiorDepthConditions) {
        if (isBnd[bndId]) continue;
        if (isMergeBnd[bndId]) continue;
      }
      bool corrFound = true;
      if (used[bndId])
        continue;  // only single correspondence for boundary vertex - this is
                   // currently necessary because of asActive

      const Vector3d &vc = VCurr.row(bndId);
      const int x = static_cast<int>(vc(0)), y = static_cast<int>(vc(1));
      const RowVector2d vci(x, y);

      int faceId = -1;
      if (x >= 0 && y >= 0 && x < frameBuffer.w && y < frameBuffer.h)
        faceId = frameBuffer(x, y, regionIdMesh);
      int corrId = -1;
      double min = numeric_limits<double>::infinity();

      if (faceId != -1) {
        fora(i, 0, 3) {
          int vId = FCurr(faceId, i);
          if (isBnd[vId]) {
            corrId = -1;
            break;
          }
          if (interiorDepthConditions) {
            if (isMergeBnd[vId]) {
              corrId = -1;
              break;
            }
          }
          if (used[vId]) continue;
          const double d = (VCurr.row(vId).head(2) - vci).norm();
          if (d < min) {
            min = d;
            corrId = vId;
          }
        }
      }

      if (corrId == -1) corrFound = false;
      if (min > searchThreshold) corrFound = false;

      if (corrFound) {
        // activate constraints that are not fulfilled
        bool isActive = false;
        const Vector3d &vCorr = VCurr.row(corrId);
        if (sign * vc(2) - sign * vCorr(2) > 0) {  // comparing z-coordinate
          asActive[bndId] = corrId;
          isActive = true;
        }

        if (!isActive) {
          // some constraints may be fulfilled but still active, check if it is
          // active
          const auto &it = asActive.find(bndId);
          if (it != asActive.end()) {
            isActive = true;

            // constraint is fulfilled but deactivate it if the correspondence
            // for the current bnd vertex has changed
            const int corrIdAS = it->second;
            if (corrIdAS != corrId) {
              isActive = false;
              asActive.erase(it);
            }
          }
        }

        if (isActive) {
          // add to the list of inequalities that will be satisfied
          ineqCorrs.push_back(make_tuple(bndId, corrId, sign));
        }

        used[corrId] = true;
        used[bndId] = true;
      }
      continue;
    }
  }

  // deactivate constraints that do not have a correspondence
  set<int> hasCorr;
  for (auto &el : ineqCorrs) hasCorr.insert(get<0>(el));
  for (auto it = asActive.begin(); it != asActive.end(); ++it) {
    if (hasCorr.find(it->first) == hasCorr.end()) {
      it = asActive.erase(it);
      if (it == asActive.end()) break;
    }
  }

  int numEqs = 0;
  vector<Triplet<double>> tripletsEq;
  if (armpitsStitchingInJointOptimization) {
    // create equality matrix
    numEqs = mergeArmpitsCorrs.size();
    Aeq = SparseMatrix<double>(numEqs, VCurr.rows());
    Beq = VectorXd(numEqs);
    Beq.fill(0);
    tripletsEq.reserve(2 * numEqs);
    int c = 0;
    for (auto &el : mergeArmpitsCorrs) {
      const int a = get<0>(el);
      const int b = get<1>(el);
      const int sign = get<2>(el);
      tripletsEq.push_back(Triplet<double>(c, a, sign * 1));   // boundary
      tripletsEq.push_back(Triplet<double>(c, b, sign * -1));  // mesh
      c++;
    }
    Aeq.setFromTriplets(tripletsEq.begin(), tripletsEq.end());
  } else {
    Aeq = SparseMatrix<double>();
    Beq = VectorXd();
  }

  // create inequality matrix
  int numIneqs = ineqCorrs.size();
  Aieq = SparseMatrix<double>(numIneqs, VCurr.rows());
  Bieq = VectorXd(numIneqs);
  Bieq.fill(0);
  vector<Triplet<double>> tripletsIeq, tripletsAll;
  tripletsIeq.reserve(2 * numIneqs);
  int c = 0;
  for (auto &el : ineqCorrs) {
    const int a = get<0>(el);
    const int b = get<1>(el);
    const int sign = get<2>(el);
    tripletsIeq.push_back(Triplet<double>(c, a, sign * 1));   // boundary
    tripletsIeq.push_back(Triplet<double>(c, b, sign * -1));  // mesh
    c++;
  }
  Aieq.setFromTriplets(tripletsIeq.begin(), tripletsIeq.end());

  // create combined (ineqs + eqs) matrix
  int numAll = numIneqs + numEqs;
  tripletsAll = tripletsIeq;
  tripletsAll.insert(tripletsAll.end(), tripletsEq.begin(), tripletsEq.end());
  AeqAll = SparseMatrix<double>(numAll, VCurr.rows());
  BeqAll = VectorXd(numAll);
  BeqAll.fill(0);
  AeqAll.setFromTriplets(tripletsAll.begin(), tripletsAll.end());

  // reindexing for active set
  reindex.resize(numIneqs, -1);
  forlist(i, ineqCorrs) reindex[i] = get<0>(ineqCorrs[i]);
}

void DefEngARAPL::precompute(const Def3D &def, Mesh3D &mesh) {
  if (!checkData(mesh)) return;

  const auto &cps = def.getCPs();
  const int nCPs = cps.size();

  MatrixXd &VRest = mesh.VRest, &VCurr = mesh.VCurr;
  MatrixXi &F = mesh.F;
  if (VCurr.rows() != VRest.rows()) {
    VCurr = VRest;
  }

  // temporal smoothing - prepare
  if (tempSmoothingSteps > 0 && VPrevsZ.size() < tempSmoothingSteps + 1) {
    VPrevsZ.resize(tempSmoothingSteps + 1, VCurr.col(2));
  }

  // compute constraints
  VectorXi b;
  const int n = VRest.rows();
  Eigen::VectorXd constraints(n);
  constraints.fill(1.0 - rigidity);
  for (const auto &it : cps) {
    const Def3D::CP &cp = *it.second;
    const int ptId = cp.ptId;
    const double w = 1;
    constraints(ptId) = w;
    VCurr.row(ptId) = cp.pos;
  }

  // compute lambdas
  lambda = SparseMatrix<double>(n, n);
  lambdaInv = SparseMatrix<double>(n, n);
  SparseMatrix<double> I2(n, n);
  I2.setIdentity();
  lambda = I2 * -(1.0 - rigidity);  // do not constrain
  if (!cpOptimizeForXY) {
    forlist(i, constraints) lambda.coeffRef(i, i) =
        -constraints(i);  // constrain
  }
  lambdaInv = I2 + lambda;

  lambda2 = SparseMatrix<double>(n, n);
  lambda2 =
      I2 * -(1.0 - rigidity);  // do not constrain, i.e. let z-axis be free
  if (!cpOptimizeForZ) {
    forlist(i, constraints) lambda2.coeffRef(i, i) =
        -constraints(i);  // constrain, i.e. fix z-axis
  }
  lambda2Inv = I2 + lambda2;

  if (I.rows() != n) {
    I = SparseMatrix<double>(n, n);
    fora(i, 0, n) I.insert(i, i) = 1;  // all vertices
  }

  if (L.rows() != n) {
    cotmatrix(VRest, F, L);
    massmatrix(VRest, F, MASSMATRIX_TYPE_DEFAULT, M);
    invert_diag(M, Minv);
  }
  if (K.rows() == 0 || CSM.rows() == 0) {
    prepare(VRest, F);
  }

  SparseMatrix<double> Q = -(lambdaInv * L + lambda);
  min_quad_with_fixed_precompute(Q, VectorXi(), Aeq, false, data);
  SparseMatrix<double> Q2 = -(lambda2Inv * L + lambda2);
  min_quad_with_fixed_precompute(Q2, VectorXi(), AeqAll, false, dataQP);

  prevCpsChanged = def.getCp2ptChangedNum();
}

double DefEngARAPL::deform(Def3D &def, Mesh3D &mesh) {
  if (!checkData(mesh)) return numeric_limits<double>::infinity();

  MatrixXd &VRest = mesh.VRest, &VCurr = mesh.VCurr;
  MatrixXi &F = mesh.F;

  const auto &cps = def.getCPs();

  // check if control points changed, if so, recompute
  bool recompute = false;
  bool cpsChanged = def.getCp2ptChangedNum() != prevCpsChanged;
  if (cpsChanged) recompute = true;

  if (VCurr.rows() != VRest.rows()) recompute = true;
  if (recompute) {
    precompute(def, mesh);
  }

  if (solveForZ) {
    const MatrixXd &VCurrActiveSet2 =
        (VCurrActiveSet.rows() > 0 && VCurrActiveSet.rows() == VCurr.rows())
            ? VCurrActiveSet
            : VCurr;
    reindex.clear();
    prepareActiveSetEqs(VCurrActiveSet2, F);
  }

  if (armpitsStitchingInJointOptimization) {
    SparseMatrix<double> Q = -(lambdaInv * L + lambda);
    min_quad_with_fixed_precompute(Q, VectorXi(), Aeq, false, data);
  }
  if (solveForZ) {
    SparseMatrix<double> Q2 = -(lambda2Inv * L + lambda2);
    min_quad_with_fixed_precompute(Q2, VectorXi(), AeqAll, false, dataQP);
  }

  // update positions according to CPs
  for (const auto &it : cps) {
    const Def3D::CP &cp = *it.second;
    const int p = cp.ptId;
    if (!cpOptimizeForXY)
      VCurr.row(p).head(2) = cp.pos.head(2);       // update x,y only
    if (!cpOptimizeForZ) VCurr(p, 2) = cp.pos(2);  // update z
  }

  MatrixXd bc;
  VectorXd sol;
  MatrixXd solsXY, solsZ;
  MatrixXd VcurXY = VCurr;
  MatrixXd VcurZ = VCurr;

  // solve (deformation for XY)
  fora(i, 0, maxIter)
      solveARAP(data, lambda, lambdaInv, K, VRest, Beq, VcurXY, solsXY);
  if (solveForZ) {
    // solve (deformation for Z & relative depths for Z)
    fora(i, 0, maxIter)
        solveARAP(dataQP, lambda2, lambda2Inv, K, VRest, BeqAll, VcurZ, solsZ);
  }

  VCurr.leftCols(2) = VcurXY.leftCols(2);
  if (solveForZ) {
    VCurr.col(2) = VcurZ.col(2);
    sol = solsZ.col(2);
  }

  if (solveForZ) {
    // active set
    fora(i, VCurr.rows(), sol.rows()) {
      if (i - VCurr.rows() >= reindex.size())
        break;  // sol contains lagrangian values for both inequalities and
                // equalities, skip equalities
      const int id = reindex[i - VCurr.rows()];
      if (sol(i) < igl::DOUBLE_EPS) {
        // deactivate satisfied constraints
        if (id != -1) asActive.erase(id);
      }
    }
  }

  // temporal smoothing
  if (tempSmoothingSteps > 0) {
    VPrevsZ.push_front(VCurr.col(2));
    fora(i, 1, tempSmoothingSteps + 1) { VCurr.col(2) += VPrevsZ[i]; }
    VCurr.col(2) /= (tempSmoothingSteps + 1);
    VPrevsZ.pop_back();
  }

  // compute difference to the last mesh state
  double diff = numeric_limits<double>::infinity();
  if (VPrev.size() == VCurr.size() && M.rows() == VCurr.rows()) {
    diff = (M * (VCurr - VPrev).rowwise().norm()).sum() / M.sum();
  }
  VPrev = VCurr;

  return diff;
}
