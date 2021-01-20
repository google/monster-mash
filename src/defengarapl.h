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

#ifndef DEFENGARAPL_H
#define DEFENGARAPL_H

// clang-format off
#include <Eigen/Core>
#include <Eigen/src/SparseCore/SparseSolverBase-mod.h>
#include <Eigen/Sparse>
#include <Eigen/src/OrderingMethods/Amd-mod.h>
#include <Eigen/src/OrderingMethods/Ordering-mod.h>
#include <Eigen/src/SparseCholesky/SimplicialCholesky.h>
#include <Eigen/src/SparseCholesky/SimplicialCholesky_impl-mod.h>
#include <igl/min_quad_with_fixed.h>
#include <miscutils/def3d.h>
#include <miscutils/mesh3d.h>
// clang-format on

#include <deque>
#include <unordered_map>

class DefEngARAPL {
 public:
  DefEngARAPL();
  virtual ~DefEngARAPL();
  virtual double deform(Def3D &def, Mesh3D &mesh);
  virtual void precompute(const Def3D &def, Mesh3D &mesh);

 private:
  bool checkData(const Mesh3D &mesh);
  static bool checkData(const Eigen::MatrixXd &VCurr,
                        const Eigen::MatrixXd &VRest, const Eigen::MatrixXi &F);
  void prepare(const Eigen::MatrixXd &V, const Eigen::MatrixXi &F);
  void solveARAP(igl::min_quad_with_fixed_data<double> &data,
                 const Eigen::SparseMatrix<double> &lambda,
                 const Eigen::SparseMatrix<double> &lambdaInv,
                 const Eigen::SparseMatrix<double> &K,
                 const Eigen::MatrixXd &VRest, const Eigen::VectorXd &Beq,
                 Eigen::MatrixXd &VCurr, Eigen::MatrixXd &sol);
  void prepareActiveSetEqs(const Eigen::MatrixXd &VCurr,
                           const Eigen::MatrixXi &FCurr);

 public:
  std::vector<std::tuple<int, int, int>> ineqRegionConds;
  std::vector<std::tuple<int, int, int>> mergeArmpitsCorrs;
  std::vector<std::vector<int>> bnds, partsIds;
  std::vector<int> mergeBnd;
  int tempSmoothingSteps = 0;
  int maxIter = 1;
  int searchThreshold = 5;
  bool cpOptimizeForXY = true, cpOptimizeForZ = false;
  bool armpitsStitchingInJointOptimization = false;
  bool interiorDepthConditions = false;
  bool solveForZ = true;
  double rigidity;
  Eigen::SparseMatrix<double> L, M, Minv;

 private:
  long prevCpsChanged = -1;
  Eigen::SparseMatrix<double> lambda, lambdaInv, lambda2, lambda2Inv;
  Eigen::MatrixXd VPrev;
  igl::min_quad_with_fixed_data<double> data, dataQP;
  Eigen::SparseMatrix<double> K, CSM;
  Eigen::SparseMatrix<double> Aeq, Aieq, AeqAll, I;
  Eigen::VectorXd Beq, Bieq, BeqAll;
  std::unordered_map<int, int> asActive;
  std::vector<int> reindex;
  Eigen::MatrixXd VCurrActiveSet;
  std::deque<Eigen::MatrixXd> VPrevsZ;
};

#endif  // DEFENGARAPL_H
