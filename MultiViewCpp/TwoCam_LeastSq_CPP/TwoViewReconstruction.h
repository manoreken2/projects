#pragma once

#include <Eigen/Dense>
#include "Triangulation.h"
#include <vector>
#include "Common.h"



namespace TwoViewReconstruction {
   const double Default_f0 = 600.0;
   const int Max_Iteration = 30;
   const double Convergence_EPS = 1e-5;
   const double Large_Number = 1e99;

   const Vector3d ZeroVec3 = Vector3d::Zero();
   const Matrix3d I3 = Matrix3d::Identity();
   const Vector3d Vec_k(0.0, 0.0, 1.0);

   bool focal_length_computation(const Matrix3d& F,
                                 double* nf0,
                                 double* nf1,
                                 double f0 = Default_f0);
   
   bool focal_length_computation(const Vector9d& theta,
                                 double* nf0,
                                 double* nf1,
                                 double f0 = Default_f0);
   
   bool motion_parameter_computation(
       const Matrix3d& F,
        double nf0,
        double nf1,
        std::vector<Vector2d>& pos0,
        std::vector<Vector2d>& pos1,
        int Num,
        Matrix3d& R,
        Vector3d& t,
        double def_f0 = Default_f0);

   bool motion_parameter_computation(
       const Vector9d& theta,
        double nf0,
        double nf1,
        std::vector<Vector2d>& pos0,
       std::vector < Vector2d>& pos1,
        int Num,
        Matrix3d& R,
        Vector3d& t,
        double def_f0 = Default_f0);

   bool reconstruction(
       std::vector<Vector2d> &pos0,
       std::vector < Vector2d>& pos1,
        int Num,
        const Vector9d& theta,
        const Matrix3d& R,
        const Vector3d& t,
        double f,
        double fp,
        Vector3d X[],
        int Max_Iter = Max_Iteration,
        double Conv_EPS = Convergence_EPS,
        double f0 = Default_f0);

   bool reconstruction(
       std::vector<Vector2d>& pos0,
       std::vector < Vector2d>& pos1,
        int Num,
        const Matrix3d& F,
        const Matrix3d& R,
        const Vector3d& t,
        double f,
        double fp,
        Vector3d X[],
        int Max_Iter = Max_Iteration,
        double Conv_EPS = Convergence_EPS,
        double f0 = Default_f0);

}

