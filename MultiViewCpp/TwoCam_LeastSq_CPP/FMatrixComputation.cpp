//
// Least Squares for Fundamental Matrix Computation
//

#include "FMatrixComputation.h"
#include "MT.h"

using namespace Eigen;

namespace {

}

namespace FMatrixComputation {
    /// <summary>
    /// 値xを2乗して戻す。
    /// </summary>
    double sqr(double x) {
        return x * x;
    }

    // Convert two 2-vectors into 9-vector
    Vector9d
        convert_vec9(const Vector2d& p0, const Vector2d& p1, double f0)
    {
        Vector9d t;

        t <<
            p0(0) * p1(0), p0(0)* p1(1), f0* p0(0),
            p0(1)* p1(0), p0(1)* p1(1), f0* p0(1),
            f0* p1(0), f0* p1(1), f0* f0;

        return t;
    }

    // Convert four 2-vectors into 9-vector
    Vector9d
        convert_vec9_st(const Vector2d& p0h, const Vector2d& p1h,
            const Vector2d& p0t, const Vector2d& p1t,
            double f0)
    {
        Vector9d t;

        t <<
            p0h(0) * p1h(0) + p1h(0) * p0t(0) + p0h(0) * p1t(0),
            p0h(0)* p1h(1) + p1h(1) * p0t(0) + p0h(0) * p1t(1),
            f0* (p0h(0) + p0t(0)),
            p0h(1)* p1h(0) + p1h(0) * p0t(1) + p0h(1) * p1t(0),
            p0h(1)* p1h(1) + p1h(1) * p0t(1) + p0h(1) * p1t(1),
            f0* (p0h(1) + p0t(1)),
            f0* (p1h(0) + p1t(0)),
            f0* (p1h(1) + p1t(1)),
            f0* f0;

        return t;
    }

    // Make covariance matrix from two 2-vectors
    Matrix9d
        make_cov_mat(const Vector2d& p0, const Vector2d& p1, double f0)
    {
        Matrix9d V0 = ZeroMat9;
        double f02 = sqr(f0);

        V0(0, 0) = sqr(p0(0)) + sqr(p1(0));
        V0(1, 1) = sqr(p0(0)) + sqr(p1(1));
        V0(2, 2) = V0(5, 5) = V0(6, 6) = V0(7, 7) = f02;
        V0(3, 3) = sqr(p0(1)) + sqr(p1(0));
        V0(4, 4) = sqr(p0(1)) + sqr(p1(1));

        V0(0, 1) = V0(1, 0) = V0(3, 4) = V0(4, 3) = p1(0) * p1(1);
        V0(0, 2) = V0(2, 0) = V0(3, 5) = V0(5, 3) = f0 * p1(0);
        V0(0, 3) = V0(3, 0) = V0(1, 4) = V0(4, 1) = p0(0) * p0(1);
        V0(0, 6) = V0(6, 0) = V0(1, 7) = V0(7, 1) = f0 * p0(0);
        V0(1, 2) = V0(2, 1) = V0(4, 5) = V0(5, 4) = f0 * p1(1);
        V0(3, 6) = V0(6, 3) = V0(4, 7) = V0(7, 4) = f0 * p0(1);

        return V0;
    }

    // convert 9-vector to 33-matrix
    Matrix3d
        convert_mat3(const Vector9d& th)
    {
        Matrix3d F;

        F << th(0), th(1), th(2), th(3), th(4), th(5), th(6), th(7), th(8);

        return F;
    }

    // convert 33-matrix to 9-vector
    Vector9d
        convert_vec9(const Matrix3d& F)
    {
        Vector9d theta;

        theta << F(0, 0), F(0, 1), F(0, 2),
            F(1, 0), F(1, 1), F(1, 2),
            F(2, 0), F(2, 1), F(2, 2);

        return theta;
    }

    // convert theta^\dag
    Vector9d
        theta_dag(const Vector9d& th)
    {
        Vector9d     thdag;

        thdag <<
            th(4) * th(8) - th(7) * th(5),
            th(5)* th(6) - th(8) * th(3),
            th(3)* th(7) - th(6) * th(4),
            th(7)* th(2) - th(1) * th(8),
            th(8)* th(0) - th(2) * th(6),
            th(6)* th(1) - th(0) * th(7),
            th(1)* th(5) - th(4) * th(2),
            th(2)* th(3) - th(5) * th(0),
            th(0)* th(4) - th(3) * th(1);

        return thdag;
    }


    void
        sample_unique_indices(
            int idx[],           // index array:                 OUTPUT
            int SampleNum,       // Number of sample indices:    INPUT
            int DataNum          // Number of Data:              INPUT
        )
    {
        int  x;

        for (int i = 0; i < SampleNum; i++)
        {
            // check
            do
            {
                x = (int)(DataNum * genrand_real2());
            } while (!check_unique(idx, i, x));

            idx[i] = x;
        }
    }

    bool
        least_squares(
            Vector2d pos0[],
            Vector2d pos1[],
            int      Num,
            Vector9d& theta,
            double    f0)
    {
        Vector9d     Xi;
        Matrix9d     M;

        // Initialization
        M = ZeroMat9;

        // Calc Moment Matrix
        for (int al = 0; al < Num; al++)
        {
            Xi = convert_vec9(pos0[al], pos1[al], f0);
            M += Xi * Xi.transpose();
        }
        M /= (double)Num;

        // Compute eigen vector corresponding to the smallest eigen value
        SelfAdjointEigenSolver<Matrix9d> ES(M);
        if (ES.info() != Success) return false;
        theta = ES.eigenvectors().col(0);

        return true;
    }

    static void
        PrintVec9H(const char* theta_name, Vector9d& t)
    {
        printf("%s=%e %e %e %e %e %e %e %e %e\n",
            theta_name,
            t[0],
            t[1],
            t[2],
            t[3],
            t[4],

            t[5],
            t[6],
            t[7],
            t[8]);
    }

    // Optimal rank correction of fundamental matrix
    bool
    optimal_rank_correction(
            const Vector9d& theta0,
            Vector9d& theta,
            Vector2d pos0[],
            Vector2d pos1[],
            int Num,
            double Conv_EPS,
            double f0
        )
    {
        Matrix9d     M, Pth;
        Vector9d     Xi, thdag, PthXi;
        Matrix9d     V0, V0th;
        double       d;

        theta = theta0;
        // Calc Projection Matrix
        Pth = I9 - theta * theta.transpose();

        // Calc Matrices
        M = ZeroMat9;
        for (int al = 0; al < Num; al++)
        {
            Xi = convert_vec9(pos0[al], pos1[al], f0);
            V0 = make_cov_mat(pos0[al], pos1[al], f0);
            PthXi = Pth * Xi;
            M += PthXi * PthXi.transpose() / theta.dot(V0 * theta);
        }
        M /= (double)Num;

        // Compute Eigen Values and Vectors
        SelfAdjointEigenSolver<Matrix9d> ES(M);
        if (ES.info() != Success) return false;

        // Compute V0[theta] (generalized inverse with rank 8)
        V0th = ZeroMat9;
        for (int i = 1; i < 9; i++) // except i=0, which means the smallest eigen value
        {
            Vector9d eVec = ES.eigenvectors().col(i);
            eVec.normalize();
            //printf("eVal=%e ", ES.eigenvalues()(i));
            //PrintVec9H("eVec", eVec);

            V0th += ES.eigenvectors().col(i) * ES.eigenvectors().col(i).transpose()
                / ES.eigenvalues()(i);
        }
        V0th /= (double)Num;

        // update loop
        do
        {
            // theta^\dag: equivalent to the cofactor matirx of F.
            thdag = theta_dag(theta);

            // update theta
            theta -= thdag.dot(theta) * V0th * thdag
                / (3.0 * thdag.dot(V0th * thdag));
            theta.normalize();

            // update Ptheta
            Pth = I9 - theta * theta.transpose();

            // Update V0
            V0th = Pth * V0th * Pth;

            // check det F = 0
            d = fabs(thdag.dot(theta));
        } while (d >= Conv_EPS);

        // success
        return true;
    }
}

