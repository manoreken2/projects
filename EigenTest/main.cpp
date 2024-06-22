#include <stdio.h>
#include <Eigen/Core>
#include <Eigen/Eigenvalues>
#include <Eigen/Geometry>
#include <string>
#define _USE_MATH_DEFINES
#include <math.h>

static const int STR_BUFLEN = 1024;

static std::string MatToStr(Eigen::Matrix3d& m)
{
    char s[STR_BUFLEN];
    char* p = &s[0];

    for (int y = 0; y < 3; ++y) {
        int n = snprintf(p, STR_BUFLEN - (p - s), "\t%f\t%f\t%f\n", m(y, 0), m(y, 1), m(y, 2));
        p += n;
    }

    return std::string(s);
}

static std::string VecToStr(Eigen::Vector3cd& v)
{
    char s[STR_BUFLEN];
    sprintf_s(s, "\t%f+%fi\n\t%f+%fi\n\t%f+%fi\n", v(0).real(), v(0).imag(), v(1).real(), v(1).imag(), v(2).real(), v(2).imag());

    return std::string(s);
}

static Eigen::Matrix3d RotMat3X(double r)
{
    Eigen::Matrix3d m;
    m << 1, 0,       0,
         0, cos(r), -sin(r),
         0, sin(r),  cos(r);

    return m;
}

static Eigen::Matrix3d RotMat3Y(double r)
{
    Eigen::Matrix3d m;
    m << cos(r), 0, sin(r),
         0,      1, 0,
        -sin(r), 0, cos(r);

    return m;
}

int main(void)
{
    Eigen::Matrix3d rx = RotMat3X(M_PI / 2);
    Eigen::Matrix3d ry = RotMat3Y(M_PI / 2);

    Eigen::Matrix3d m = ry * rx;

    printf("m=%s\n", MatToStr(m).c_str());

    Eigen::EigenSolver<Eigen::Matrix3d> es;
    es.compute(m, true);

    {
        auto lambda = es.eigenvalues();
        auto ev = es.eigenvectors();

        for (int i = 0; i < ev.cols(); ++i) {
            Eigen::Vector3cd v = ev.col(i);
            auto s = VecToStr(v);
            printf("l  %d=\t%f+%fi\n", i, lambda[i].real(), lambda[i].imag());
            printf("ev %d=%s\n", i, s.c_str());
        }
    }

    return 0;
}

