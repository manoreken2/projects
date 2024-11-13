#pragma once

#include <Eigen/Dense>
using namespace Eigen;

typedef Matrix<double, 9, 9> Matrix9d;
typedef Matrix<double, 9, 3> Matrix93d;
typedef Matrix<double, 9, 1> Vector9d;
typedef Matrix<double, 7, 7> Matrix7d;
typedef Matrix<double, 7, 1> Vector7d;
typedef Matrix<double, 2, 3> Matrix23d;
typedef Matrix<double, 3, 4> Matrix34d;
typedef Matrix<double, 4, 3> Matrix43d;

void
PrintVec9(const char* theta_name, Vector9d& t);

void
PrintVec3(const char* name, Vector3d& t);

void
PrintMat3(const char* name, Matrix3d& t);

void
PrintThetaAsF(const char* name, Vector9d& t);


inline double sqr(double x) {
    return x * x;
}


inline int
sgn(double x)
{
    const double Almost0 = 1e-8;

    if (fabs(x) < Almost0)
        return 0;
    else if (x < 0.0)
        return -1;
    else
        return 1;
}


// 値xが、配列idx内に存在する場合false
// すなわち、値xが、配列idx内に存在しないuniqueな値のときtrue
inline bool
check_unique(int idx[], int Num, int x)
{
    for (int i = 0; i < Num; i++)
    {
        if (idx[i] == x)
            return false;
    }

    return true;
}
