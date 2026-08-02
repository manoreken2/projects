#include "Triangulation.h"
#include <iostream>

using namespace Eigen;

namespace {
   
    Vector9d
    convert_vec9_st(const Vector2d &p0h, const Vector2d &p1h,
                    const Vector2d &p0t, const Vector2d &p1t,
                    double f0)
    {
       Vector9d t;

       t <<
          p0h(0)*p1h(0) + p1h(0)*p0t(0) + p0h(0)*p1t(0),
          p0h(0)*p1h(1) + p1h(1)*p0t(0) + p0h(0)*p1t(1),
    //      p0h(0)*p1h(0) + p1h(0)*p0t(0) + p1h(0)*p1t(0),
    //      p0h(0)*p1h(1) + p1h(1)*p0t(0) + p1h(0)*p1t(1),
          f0*(p0h(0) + p0t(0)),
          p0h(1)*p1h(0) + p1h(0)*p0t(1) + p0h(1)*p1t(0),
          p0h(1)*p1h(1) + p1h(1)*p0t(1) + p0h(1)*p1t(1),
    //      p0h(1)*p1h(0) + p1h(0)*p0t(1) + p1h(1)*p1t(0),
    //      p0h(1)*p1h(1) + p1h(1)*p0t(1) + p1h(1)*p1t(1),
          f0*(p0h(1) + p0t(1)),
          f0*(p1h(0) + p1t(0)),
          f0*(p1h(1) + p1t(1)),
          f0*f0;

       return t;
    }

    Matrix9d
    make_cov_mat(const Vector2d &p0, const Vector2d &p1, double f0)
    {
       Matrix9d V0 = Triangulation::ZeroMat9;
       double f02 = sqr(f0);

       V0(0,0) = sqr(p0(0)) + sqr(p1(0));
       V0(1,1) = sqr(p0(0)) + sqr(p1(1));
       V0(2,2) = V0(5,5) = V0(6,6) = V0(7,7) = f02;
       V0(3,3) = sqr(p0(1)) + sqr(p1(0));
       V0(4,4) = sqr(p0(1)) + sqr(p1(1));

       V0(0,1) = V0(1,0) = V0(3,4) = V0(4,3) = p1(0) * p1(1);
       V0(0,2) = V0(2,0) = V0(3,5) = V0(5,3) = f0 * p1(0);
       V0(0,3) = V0(3,0) = V0(1,4) = V0(4,1) = p0(0) * p0(1);
       V0(0,6) = V0(6,0) = V0(1,7) = V0(7,1) = f0 * p0(0);
       V0(1,2) = V0(2,1) = V0(4,5) = V0(5,4) = f0 * p1(1);
       V0(3,6) = V0(6,3) = V0(4,7) = V0(7,4) = f0 * p0(1);

       return V0;
    }

    inline Matrix43d
        make_matrix_T(const Matrix34d& P0, const Matrix34d& P1,
            const Vector2d& p0, const Vector2d& p1,
            double f0)
    {
        Matrix43d    T;

        T <<
            f0 * P0(0, 0) - p0(0) * P0(2, 0), f0* P0(0, 1) - p0(0) * P0(2, 1), f0* P0(0, 2) - p0(0) * P0(2, 2),
            f0* P0(1, 0) - p0(1) * P0(2, 0), f0* P0(1, 1) - p0(1) * P0(2, 1), f0* P0(1, 2) - p0(1) * P0(2, 2),
            f0* P1(0, 0) - p1(0) * P1(2, 0), f0* P1(0, 1) - p1(0) * P1(2, 1), f0* P1(0, 2) - p1(0) * P1(2, 2),
            f0* P1(1, 0) - p1(1) * P1(2, 0), f0* P1(1, 1) - p1(1) * P1(2, 1), f0* P1(1, 2) - p1(1) * P1(2, 2);

        return T;
    }

    inline Vector4d
        make_vector_p(const Matrix34d& P0, const Matrix34d& P1,
            const Vector2d& p0, const Vector2d& p1,
            double f0)
    {
        Vector4d     p;

        p <<
            f0 * P0(0, 3) - p0(0) * P0(2, 3),
            f0* P0(1, 3) - p0(1) * P0(2, 3),
            f0* P1(0, 3) - p1(0) * P1(2, 3),
            f0* P1(1, 3) - p1(1) * P1(2, 3);

        return p;
    }

}

// Triangulation using least squares from two camera matrices
Vector3d
Triangulation::least_squares(
    const Vector2d& pos0,
    const Vector2d& pos1,
    const Matrix34d& P0,
    const Matrix34d& P1,
    double    f0)
{
    Matrix43d    T;
    Vector4d     p;
    Vector3d     mTtp;
    Matrix3d     TtT;
    Vector3d     X;

    // constructer of LU
    FullPivLU<Matrix3d> LU;

    T = make_matrix_T(P0, P1, pos0, pos1, f0);
    p = make_vector_p(P0, P1, pos0, pos1, f0);
    TtT = T.transpose() * T;
    mTtp = -T.transpose() * p;

    // compute least squares solution by LU decomposition
    LU.compute(TtT);
    X = LU.solve(mTtp);

    return X;
}

// Triangulation using least squares from two camera matrices
bool
Triangulation::least_squares(
    std::vector<Vector2d>& pos0,
    std::vector < Vector2d>& pos1,
    int      Num,
    const Matrix34d& P0,
    const Matrix34d& P1,
    Vector3d X[],
    double    f0)
{
    for (int al = 0; al < Num; al++)
        X[al] = least_squares(pos0[al], pos1[al], P0, P1, f0);

    return true;
}

double
Triangulation::optimal_correction(
   const Vector2d& pos0,
   const Vector2d& pos1,
   const Vector9d &theta,
   Vector2d& npos0,
   Vector2d& npos1,
   int      Max_Iter,
   double   Conv_EPS,
   double   f0
)
{
   Vector9d     Xst;                 // Xi*
   Matrix9d     V0;
   Vector2d     p0h, p1h, p0t, p1t;
   double       S0, S;
   double       c;
   int          count;
   Matrix23d    thM0, thM1;
   Vector3d     t0, t1;

   // set Matrix
   thM0 << theta(0), theta(1), theta(2), theta(3), theta(4), theta(5);
   thM1 << theta(0), theta(3), theta(6), theta(1), theta(4), theta(7);
   
   // Initialization
   p0h = pos0;
   p1h = pos1;
   p0t = p1t = ZeroVec2;

   // set large number to S0
   S0 = Large_Number;
   
   // Update by iteration
   count = 0;
   do
   {
      // Set Xi and V0
      V0 = make_cov_mat(p0h, p1h, f0);
      Xst = convert_vec9_st(p0h, p1h, p0t, p1t, f0);

      // compute ``tilde''s
      // t0 << p0h(0), p0h(1), f0;
      // t1 << p1h(0), p1h(1), f0;
      t0 << p0h, f0;
      t1 << p1h, f0;
      c = theta.dot(Xst) / (theta.dot(V0*theta));

      p0t = c * thM0 * t1;
      p1t = c * thM1 * t0;
      
      // update ``hat''s
      p0h = pos0 - p0t;
      p1h = pos1 - p1t;

      // compute residual
      S = p0t.squaredNorm() + p1t.squaredNorm();
      
      // check convergence
      if (fabs(S - S0) < Conv_EPS)  break;
      S0 = S;
   }
   while (++count < Max_Iter);
         
   // Maximum Iteration
   if (count >= Max_Iter) // failed
   {
      npos0 = npos1 = ZeroVec2;
   }
   else
   {
      // set corrected data
      npos0 = p0h;
      npos1 = p1h;
   }

   return S;
}

bool
Triangulation::optimal_correction(
   std::vector<Vector2d> &pos0,     // Data:                    INPUT
   std::vector<Vector2d> &pos1,     // Data:                    INPUT
   int      Num,      // Number of Data:            INPUT
   const Vector9d &theta,   // Fundamental Matrix:  INPUT
   std::vector<Vector2d> &npos0,     // Data:                   OUTPUT
   std::vector<Vector2d> &npos1,     // Data:                   OUTPUT
   int      Max_Iter, // Max. Iteration:            INPUT w. default
   double   Conv_EPS, // Threshold for convergence: INPUT w. default
   double   f0        // Default focal length:      INPUT w. default
)
{
   double S;
   
   for (int al = 0; al < Num; al++)
   {
      S = optimal_correction(pos0[al], pos1[al], theta, npos0[al], npos1[al],
                             Max_Iter, Conv_EPS, f0);
   }

   return true;
}
