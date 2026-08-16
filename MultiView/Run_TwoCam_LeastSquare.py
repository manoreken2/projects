# 3章 手順3.1 2つの画像の対応点から基礎行列Fを求める。

# python Run_TwoCam_LeastSquare.py --matched_point2d_csv tmp/0001_0002.csv

from Common import (
    AdjustTwoPoints,
    CSV_Read_TwoCam_MatchedPointList,
    Epipolar_Constraint_Error,
    FToTheta,
    InlierPointList_from_bitmap,
    PLY_Export_PointNdArray,
    PLY_Export_TwoCam,
    ThetaToF,
    Triangulation,
    TwoCam_LeastSquare,
    Reconstruct_F_from,
)
from Rank_Correction import Optimal_rank_correction
from Fundamental_to_CamParams import (
    Fundamental_to_Trans_Rot,
    Fundamental_to_FocalLength,
    Build_two_cam_P0_P1,
)

if __name__ == "__main__":
    DEFAULT_F0 = 600

    f0 = DEFAULT_F0

    pp = CSV_Read_TwoCam_MatchedPointList("tmp/op0001_0002.csv")
    N = pp.get_point_count()

    if False:
        # 実験
        F = CSV_Read_F("Chap5_GroundTruth_F.csv")
        theta = FToTheta(F)
    else:
        theta = TwoCam_LeastSquare(pp, f0)
        print(f"theta={theta}")

        theta = Optimal_rank_correction(theta, pp, f0)
        print(f"rank correction theta={theta}")

        F = ThetaToF(theta)
        print(f"least-sq optimal_rank_correction F=\n{F}")

    print(f"F={F}")

    focalLen_Cam0, focalLen_Cam1 = Fundamental_to_FocalLength(F, f0)
    print(f"Focal length = {focalLen_Cam0} {focalLen_Cam1}")

    valid_bitmap = N * [True]

    err, valid_point_count = Epipolar_Constraint_Error(
        pp, valid_bitmap, focalLen_Cam0, focalLen_Cam1, F
    )
    print(f"Epipolar Constraint error = {err}, valid point count = {valid_point_count}")

    t, R = Fundamental_to_Trans_Rot(
        F, focalLen_Cam0, focalLen_Cam1, f0, pp, valid_bitmap
    )
    print(f"trans={t}\nrot={R}")
    PLY_Export_TwoCam("tmp/Run_TwoCam_LeastSquare_TwoCam.ply", t, R)

    # Fから取得したt, R, focalLen_camを用いて、Fを再構築するテスト。
    reconF = Reconstruct_F_from(t, R, f0, focalLen_Cam0, focalLen_Cam1)
    print(f"reconstructedF={reconF}")

    P0, P1 = Build_two_cam_P0_P1(f0, focalLen_Cam0, focalLen_Cam1, t, R)

    AdjustTwoPoints(pp, valid_bitmap, theta, f0)

    xyz_list, valid_bitmap, valid_point_count = Triangulation(
        pp, valid_bitmap, f0, P0, P1
    )
    print(f"Triangulation valid_point_count={valid_point_count}")

    PLY_Export_PointNdArray(
        "tmp/Run_TwoCam_LeastSquare_InlierPoints_3D.ply",
        InlierPointList_from_bitmap(xyz_list, valid_bitmap),
    )
