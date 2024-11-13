#include "Common.h"
#include <stdio.h>

void
PrintVec9(const char* theta_name, Vector9d& t)
{
    printf("%s=\n %e\n %e\n %e\n %e\n %e\n %e\n %e\n %e\n %e\n",
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

void
PrintVec3(const char* name, Vector3d& t)
{
    printf("%s=\n %e\n %e\n %e\n",
        name,
        t[0],
        t[1],
        t[2]);
}

void
PrintMat3(const char* name, Matrix3d& t)
{
    printf("%s=\n %e\t%e\t%e\n %e\t%e\t%e\n %e\t%e\t%e\n",
        name,
        t(0, 0), t(0, 1), t(0, 2),
        t(1, 0), t(1, 1), t(1, 2),
        t(2, 0), t(2, 1), t(2, 2));
}

void
PrintThetaAsF(const char* name, Vector9d& t)
{
    /*
    # θ→F
        def ThetaToF(t) :
        F = np.zeros((3, 3))
        F[0, 0] = t[0]
        F[0, 1] = t[1]
        F[0, 2] = t[2]

        F[1, 0] = t[3]
        F[1, 1] = t[4]
        F[1, 2] = t[5]

        F[2, 0] = t[6]
        F[2, 1] = t[7]
        F[2, 2] = t[8]
    */

    printf("%s=\n %e\t%e\t%e\n %e\t%e\t%e\n %e\t%e\t%e\n",
        name,
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

