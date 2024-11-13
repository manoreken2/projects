#pragma once

#include "Common.h"

namespace FMatrixComputation {

   const double Default_f0 = 600.0;
   const int Max_Iteration = 30;
   const double Convergence_EPS = 1e-5;
   const double Large_Number = 1e99;
   const int FMat_Sample_Number = 8;
   const double Max_Sample_Count = 1000;
   const double Distance_Threshold = 2;
   const unsigned long MT_Seed = 10;

   const Vector9d ZeroVec9 = Vector9d::Zero();
   const Vector2d ZeroVec2 = Vector2d::Zero();
   const Matrix9d ZeroMat9 = Matrix9d::Zero();
   const Matrix7d ZeroMat7 = Matrix7d::Zero();
   const Matrix9d I9 = Matrix9d::Identity();

   Vector9d convert_vec9(const Vector2d &p0, const Vector2d &p1, double f0);

   Vector9d convert_vec9_st(const Vector2d &p0h, const Vector2d &p1h,
                            const Vector2d &p0t, const Vector2d &p1t,
                            double f0);
   // Make covariance matrix
   Matrix9d make_cov_mat(const Vector2d &p0, const Vector2d &p1, double f0);

   Matrix3d convert_mat3(const Vector9d &th);

   Vector9d convert_vec9(const Matrix3d &F);

   // convert theta^\dag
   Vector9d theta_dag(const Vector9d &th);

   void sample_unique_indices(int idx[], int sampleNum, int DataNum);
   
   // Rank correction of fundamental matrix by SVD: Matrix form
   Matrix3d svd_rank_correction(const Matrix3d &F);

   // Rank correction of fundamental matrix by SVD: Vector form
   Vector9d svd_rank_correction(const Vector9d &theta);

   // Optimal rank correction of fundamental matrix: vector form
   bool optimal_rank_correction(const Vector9d &theta,
                                Vector9d &ntheta,
                                Vector2d pos0[],
                                Vector2d pos1[],
                                int Num,
                                double Conv_EPS = Convergence_EPS,
                                double f0 = Default_f0);

   bool least_squares(Vector2d pos0[],
                      Vector2d pos1[],
                      int Num,
                      Vector9d& theta,
                      double f0 = Default_f0);

   // Taubin method (core)
   bool taubin_core(Vector9d Xi[],
                    Matrix9d V0[],
                    int      Num,
                    Vector9d& theta);

   // Taubin method
   bool taubin(Vector2d pos0[],
               Vector2d pos1[],
               int      Num,
               Vector9d& theta,
               double    f0 = Default_f0);
   
   // extended FNS method (core)
   bool efns_core(Vector9d &theta,
                  Vector9d Xi[],
                  Matrix9d V0[],
                  int Num,
                  int Max_Iter,
                  double Conv_EPS);

   // extended FNS method
   bool efns(Vector2d pos0[],
             Vector2d pos1[],
             int      Num,
             Vector9d &theta,
             int      Max_Iter = Max_Iteration,
             double   Conv_EPS = Convergence_EPS,
             double   f0 = Default_f0);

   // RANSAC method
   bool ransac(Vector2d pos0[],
               Vector2d pos1[],
               int Num,
               Vector9d &theta,
               unsigned long Seed = MT_Seed,
               double dist = Distance_Threshold,
               int Max_Count = Max_Sample_Count,
               double f0 = Default_f0);

   // Geometric distance minimization
   bool geometric_distance_minimization(Vector2d p0[],
                                        Vector2d p1[],
                                        int Num,
                                        const Vector9d &theta0,
                                        Vector9d &theta,
                                        int  Max_Iter = Max_Iteration,
                                        double Conv_EPS = Convergence_EPS,
                                        double f0 = Default_f0);

   // Latent Variable Method: matrix form
   // In this method, matrix F must be rank 2.
   bool latent_variable_method(const Matrix3d &F,
                               Matrix3d &newF,
                               Vector2d pos0[],
                               Vector2d pos1[],
                               int Num,
                               int Max_Iter = Max_Iteration,
                               double Conv_EPS = Convergence_EPS,
                               double f0 = Default_f0);
   
}

