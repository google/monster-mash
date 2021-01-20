Modified version of Eigen3 (https://eigen.tuxfamily.org). The changes are listed below:

- Modified Eigen/src/SparseCore/SparseSolverBase-mod.h to allow copying of SparseSolverBase instances.
- Modified Eigen/src/SparseCholesky/SimplicialCholesky_impl-mod.h to allow separation of MPL2 and non-MPL2 licensed parts of Eigen. This file is GPL v.3 licensed. Since we use EIGEN_MPL2_ONLY macro in the main Eigen branch, we modified the file to allow inclusion when EIGEN_MPL2_ONLY is defined.
- Modified Eigen/src/OrderingMethods/Amd-mod.h and Eigen/src/OrderingMethods/Ordering-mod.h for the same reason as above. The file Amd-mod.h is LGPL v.2.1 licensed.
