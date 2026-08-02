#pragma once

#include <Eigen/Dense>
#include <vector>
#include "Common.h"



namespace Triangulation {

   const double Default_f0 = 600.0;
   const int Max_Iteration = 30;
   const double Convergence_EPS = 1e-5;
   const double Large_Number = 1e99;

   const Vector2d ZeroVec2 = Vector2d::Zero();
   const Vector3d ZeroVec3 = Vector3d::Zero();
   const Matrix9d ZeroMat9 = Matrix9d::Zero();

   Vector3d least_squares(const Vector2d& pos0,
                          const Vector2d& pos1,
                          const Matrix34d &P0,
                          const Matrix34d &P1,
                          double f0 = Default_f0);

   bool least_squares(
       std::vector<Vector2d>& pos0,
       std::vector < Vector2d>& pos1,
        int Num,
        const Matrix34d &P0,
        const Matrix34d &P1,
        Vector3d X[],
        double f0 = Default_f0);

   double optimal_correction(const Vector2d& p0,
                             const Vector2d& p1,
                             const Vector9d &theta,
                             Vector2d& np0,
                             Vector2d& np1,
                             int Max_Iter = Max_Iteration,
                             double Conv_EPS = Convergence_EPS,
                             double f0 = Default_f0);
   
   bool optimal_correction(
       std::vector<Vector2d>& p0,
       std::vector<Vector2d>& p1,
       int      Num,
       const Vector9d& theta,
       std::vector<Vector2d>& np0,
       std::vector<Vector2d>& np1,
       int Max_Iter = Max_Iteration,
       double Conv_EPS = Convergence_EPS,
       double f0 = Default_f0);
}

