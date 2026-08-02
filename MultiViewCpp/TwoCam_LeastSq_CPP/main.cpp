#include <stdio.h>
#include <iostream>
#include <vector>
#include "FMatrixComputation.h"
#include "TwoViewReconstruction.h"
#include "Common.h"

using namespace Eigen;

static bool
ReadTwoCamPointList(
        const char *input_csv_filename,
        std::vector<Vector2d>& cam0,
        std::vector<Vector2d>& cam1)
{
    FILE* fp = nullptr;
    errno_t ercd = fopen_s(&fp, input_csv_filename, "rb");
    if (ercd != 0 || nullptr == fp) {
        printf("Error: input csv read failed. %s \n", input_csv_filename);
        return false;
    }

    do {
        double x0, y0, x1, y1;
        int r = fscanf_s(fp, "%lf, %lf, %lf, %lf", &x0, &y0, &x1, &y1);
        if (r != 4) {
            break;
        }

        Vector2d p0(x0, y0);
        Vector2d p1(x1, y1);

        cam0.emplace_back(p0);
        cam1.emplace_back(p1);

    } while (true);

    fclose(fp);

    printf("ReadTwoCamPointList(%s)\n", input_csv_filename);

    return true;
}

int main(void)
{
    static const char* input_csv_filename
        = "../../MultiView/twoCamPoints2.csv";
    const double f0 = 1.0;

    std::vector<Vector2d> cam0;
    std::vector<Vector2d> cam1;
    bool b = ReadTwoCamPointList(input_csv_filename, cam0, cam1);
    if (!b) {
        printf("Error: ReadTwoCamPointList failed\n");
        return 1;
    }

    int N = (int)cam0.size();

    Vector9d theta;

    FMatrixComputation::least_squares(&cam0[0], &cam1[0], N, theta, f0);
    PrintThetaAsF("F", theta);

    Vector9d oTheta;
    FMatrixComputation::optimal_rank_correction(
            theta, oTheta,
            &cam0[0], &cam1[0], N, 1e-6, f0);
    PrintThetaAsF("F optimized", oTheta);

    double fa, fb;
    TwoViewReconstruction::focal_length_computation(oTheta, &fa, &fb, f0);

    printf("Focal Length A=%f B=%f f0=%f\n", fa, fb, f0);

    Eigen::Matrix3d Rot;
    Eigen::Vector3d trans;

    TwoViewReconstruction::motion_parameter_computation(oTheta, fa, fb, cam0, cam1, N, Rot, trans, f0);

    PrintMat3("Rot", Rot);
    PrintVec3("Trans", trans);

    return 0;
}
