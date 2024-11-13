//
// 3-D reconstruction from 2 views
//

#include <iostream>
#include <vector>

#include "TwoViewReconstruction.h"

using namespace Eigen;

// unnamed namespace for local function
namespace {
   
    Matrix3d convert_mat3(const Vector9d& th)
    {
        Matrix3d F;
        F << th(0), th(1), th(2), th(3), th(4), th(5), th(6), th(7), th(8);
        return F;
    };

    Vector9d convert_vec9(const Matrix3d& F)
    {
        Vector9d theta;
        theta << F(0, 0), F(0, 1), F(0, 2), F(1, 0), F(1, 1), F(1, 2), F(2, 0), F(2, 1), F(2, 2);
        return theta;
    };

    double scalar_triple_product(const Vector3d& a, const Vector3d& b, const Vector3d& c)
    {
        return a.dot(b.cross(c));
    };

    Matrix3d cross_product(const Vector3d& t, const Matrix3d& E)
    {
        Matrix3d Tx;

        Tx << 0.0, -t(2), t(1), \
              t(2), 0.0, -t(0), \
             -t(1), t(0), 0.0;
        return Tx * E;
    };
}


bool
TwoViewReconstruction::focal_length_computation(const Matrix3d& F,
    double* nf0,
    double* nf1,
    double f0
)
{
    Vector3d     e0, e1;
    double       xi, eta;
    Vector3d     Fk, Ftk, e1k, e0k;
    double       kFk, Fk2nrm, Ftk2nrm, e1k2nrm, e0k2nrm, kFFtFk;

    // Construct of Eigen Solver
    SelfAdjointEigenSolver<Matrix3d> ES;

    // Compute epipole e0
    ES.compute(F * F.transpose());
    e0 = ES.eigenvectors().col(0);

    // Compute epipole e1
    ES.compute(F.transpose() * F);
    e1 = ES.eigenvectors().col(0);

    // compute xi and eta
    Fk = F * Vec_k;
    Ftk = F.transpose() * Vec_k;
    kFk = Vec_k.dot(Fk);
    Fk2nrm = Fk.squaredNorm();
    Ftk2nrm = Ftk.squaredNorm();
    e1k2nrm = (e1.cross(Vec_k)).squaredNorm();
    e0k2nrm = (e0.cross(Vec_k)).squaredNorm();
    kFFtFk = Vec_k.dot(F * F.transpose() * Fk);

    xi = (Fk2nrm - kFFtFk * e1k2nrm / kFk) / (e1k2nrm * Ftk2nrm - sqr(kFk));
    eta = (Ftk2nrm - kFFtFk * e0k2nrm / kFk) / (e0k2nrm * Fk2nrm - sqr(kFk));

    // imaginary focal length check
    if (xi < -1.0 || eta < -1.0)
        return false;

    // compute focal lengths
    *nf0 = f0 / sqrt(1.0 + xi);
    *nf1 = f0 / sqrt(1.0 + eta);

    return true;
}

// focal length computation (vector version)
bool
TwoViewReconstruction::focal_length_computation(const Vector9d& theta,
    double* nf0,
    double* nf1,
    double f0
)
{
    Matrix3d     F = convert_mat3(theta);

    return focal_length_computation(F, nf0, nf1, f0);
}

// 3-D reconstruction from two views
bool
TwoViewReconstruction::reconstruction(
    std::vector<Vector2d>& pos0,
    std::vector < Vector2d>& pos1,
    int Num,
    const Vector9d& theta,
    const Matrix3d& R,
    const Vector3d& t,
    double f,
    double fp,
    Vector3d X[],
    int Max_Iter,
    double Conv_EPS,
    double f0
)
{
   Matrix34d    P, Pp, T, Tp;
   DiagonalMatrix<double,3>     A(f, f, f0);
   DiagonalMatrix<double,3>     Ap(fp, fp, f0);
   std::vector<Vector2d>     np0(Num);
   std::vector<Vector2d>     np1(Num);
   double       J;

   // Projection (camera) matrices
   T.block<3,3>(0,0) = I3;
   T.block<3,1>(0,3) = ZeroVec3;
   Tp.block<3,3>(0,0) = R.transpose();
   Tp.block<3,1>(0,3) = -R.transpose() * t;
   P = A * T;
   Pp = Ap * Tp;

   // Optimally correct image points
   Triangulation::optimal_correction(pos0, pos1, Num, theta, np0, np1,
                                     Max_Iter, Conv_EPS, f0);

   // Reconstruct 3-D points from corrected image positions
   Triangulation::least_squares(np0, np1, Num, P, Pp, X, f0);

   // check sign
   J = 0.0;
   for (int al = 0; al < Num; al++)
      J += sgn(X[al](2));

   // change sign
   if (J < 0)
   {
      for (int al = 0; al < Num; al++)
         X[al] = -X[al];
   }

   return true;
}

// 3-D reconstruction from two views
bool
TwoViewReconstruction::reconstruction(
    std::vector<Vector2d>& pos0,
    std::vector < Vector2d>& pos1,
    int Num,
    const Matrix3d& F,
    const Matrix3d& R,
    const Vector3d& t,
    double f,
    double fp,
    Vector3d X[],
    int Max_Iter,
    double Conv_EPS,
    double f0)
{
   Vector9d theta = convert_vec9(F);

   return reconstruction(pos0, pos1, Num, theta, R, t, f, fp, X, Max_Iter, Conv_EPS, f0);
}

bool
TwoViewReconstruction::motion_parameter_computation(
    const Matrix3d& F,
    double nf0,
    double nf1,
    std::vector<Vector2d>& pos0,
    std::vector<Vector2d>& pos1,
    int Num,
    Matrix3d& R,
    Vector3d& t,
    double def_f0
)
{
    Matrix3d     E, K;
    Vector3d     x0, x1;
    double       J;

    // Diagonal Matrices
    DiagonalMatrix<double, 3> Df(1.0 / def_f0, 1.0 / def_f0, 1.0 / nf0);
    DiagonalMatrix<double, 3> Db(1.0 / def_f0, 1.0 / def_f0, 1.0 / nf1);

    // Set Essential Matrix
    E = Df * F * Db;

    // Compute the translation from the essential matrix
    SelfAdjointEigenSolver<Matrix3d> ES(E * E.transpose());
    t = ES.eigenvectors().col(0);

    std::cout << "t=\n" << t << std::endl;

    // check sign of translation
    J = 0.0;
    for (int al = 0; al < Num; al++)
    {
        // original code
        x0 << pos0[al](0) / nf0, pos0[al](1) / nf0, 1.0;
        x1 << pos1[al](0) / nf1, pos1[al](1) / nf1, 1.0;
        J += scalar_triple_product(t, x0, E * x1);
    }
    if (J < 0) t = -t;

    std::cout << "\nJ=" << J << std::endl;

    std::cout << "\nt=\n" << t << std::endl;

    // Compute matrix K
    K = -cross_product(t, E);

    // SVD of matrix K
    JacobiSVD<Matrix3d> SVD(K, ComputeFullU | ComputeFullV);
    Matrix3d UVt = SVD.matrixU() * SVD.matrixV().transpose();

    DiagonalMatrix<double, 3> DD(1.0, 1.0, UVt.determinant());

    std::cout << "E=\n" << E << std::endl;
    std::cout << "K=\n" << K << std::endl;
    std::cout << "U=\n" << SVD.matrixU() << std::endl;
    std::cout << "V=\n" << SVD.matrixV() << std::endl;
    PrintMat3("UVt", UVt);
    std::cout << "DD22=\n" << UVt.determinant() << std::endl;

    // compute the rotation R
    R = SVD.matrixU() * DD * (SVD.matrixV().transpose());

    std::cout << "DVht=\n" << DD * (SVD.matrixV().transpose()) << std::endl;
    PrintMat3("R", R);

    return true;
}

// Motion parameter computation: vector version
bool
TwoViewReconstruction::motion_parameter_computation(
    const Vector9d& theta,
    double nf0,
    double nf1,
    std::vector<Vector2d>& pos0,
    std::vector<Vector2d>& pos1,
    int Num,
    Matrix3d& R,
    Vector3d& t,
    double def_f0
)
{
    Matrix3d     F = convert_mat3(theta);

    return motion_parameter_computation(F, nf0, nf1, pos0, pos1, Num,
        R, t, def_f0);
}
