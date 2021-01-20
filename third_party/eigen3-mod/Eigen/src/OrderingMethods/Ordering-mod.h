
// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2012  Désiré Nuentsa-Wakam <desire.nuentsa_wakam@inria.fr>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_ORDERING_MOD_H
#define EIGEN_ORDERING_MOD_H

namespace Eigen {

#include <Eigen/src/OrderingMethods/Eigen_Colamd.h>

// #ifndef EIGEN_MPL2_ONLY

/** \ingroup OrderingMethods_Module
  * \class AMDOrdering
  *
  * Functor computing the \em approximate \em minimum \em degree ordering
  * If the matrix is not structurally symmetric, an ordering of A^T+A is computed
  * \tparam  StorageIndex The type of indices of the matrix
  * \sa COLAMDOrdering
  */
template <typename StorageIndex>
class AMDOrdering
{
  public:
    typedef PermutationMatrix<Dynamic, Dynamic, StorageIndex> PermutationType;

    /** Compute the permutation vector from a sparse matrix
     * This routine is much faster if the input matrix is column-major
     */
    template <typename MatrixType>
    void operator()(const MatrixType& mat, PermutationType& perm)
    {
      // Compute the symmetric pattern
      SparseMatrix<typename MatrixType::Scalar, ColMajor, StorageIndex> symm;
      internal::ordering_helper_at_plus_a(mat,symm);

      // Call the AMD routine
      //m_mat.prune(keep_diag());
      internal::minimum_degree_ordering(symm, perm);
    }

    /** Compute the permutation with a selfadjoint matrix */
    template <typename SrcType, unsigned int SrcUpLo>
    void operator()(const SparseSelfAdjointView<SrcType, SrcUpLo>& mat, PermutationType& perm)
    {
      SparseMatrix<typename SrcType::Scalar, ColMajor, StorageIndex> C; C = mat;

      // Call the AMD routine
      // m_mat.prune(keep_diag()); //Remove the diagonal elements
      internal::minimum_degree_ordering(C, perm);
    }
};

// #endif // EIGEN_MPL2_ONLY

} // end namespace Eigen

#endif
