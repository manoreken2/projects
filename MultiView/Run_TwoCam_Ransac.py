# 3章 手順3.8 RANSAC 2つの画像の対応点から基礎行列Fを求める。
# 実行例
# python Run_TwoCam_Ransac.py --matched_point2d_csv "twoCamPoints410_outlier10.csv" --result_3dpoints_ply "ResultPoints3D.ply"     --result_two_cam_ply "ResultTwoCamEst2r.ply"
# python Run_TwoCam_Ransac.py --matched_point2d_csv tmp/0001_0002.csv               --result_3dpoints_ply tmp/Result_0001_0002.ply --result_two_cam_ply tmp/Result_0001_0002_Est.ply

import sys
import argparse
from Common import CSV_Read_TwoCam_MatchedPointList
from Fundamental_to_CamParams import *
from Ransac_TwoCam import *
from RegressorTwoCamFNS import RegressorTwoCamFNS
from RegressorTwoCamLSQ import RegressorTwoCamLSQ
import numpy as np


def Run_TwoCam_Ransac(
    matched_point2d_csv,
    result_two_cam_ply,
    result_3dpoints_ply,
    result_two_cam_focalLengths_csv=None,
    result_cam_trans_rot_csv=None,
    regressor="fns",
    ite_count=1000,
    loss_threshold=5.0,
    close_points_ratio=0.8,
):
    DEFAULT_F0 = 600

    f0 = DEFAULT_F0

    pp = CSV_Read_TwoCam_MatchedPointList(matched_point2d_csv)
    N = pp.get_point_count()

    if regressor == "fns":
        model = RegressorTwoCamFNS(f0=f0)
    else:
        model = RegressorTwoCamLSQ(f0)

    ran = Ransac_TwoCam(
        f0=f0,
        close_points_ratio=close_points_ratio,
        ite_count=ite_count,
        loss_threshold=loss_threshold,
        model=model,
    )
    rv = ran.fit(pp)
    if rv == None:
        print(f"RANSAC failed.")
        return False

    theta = ran.get_theta()
    F = ThetaToF(theta)
    print(f"F={F}")

    focalLen_Cam0, focalLen_Cam1 = Fundamental_to_FocalLength(F, f0)
    print(f"Focal length = {focalLen_Cam0}, {focalLen_Cam1}")

    if result_two_cam_focalLengths_csv is not None:
        CSV_Write_TwoCamFocalLengths(
            result_two_cam_focalLengths_csv, focalLen_Cam0, focalLen_Cam1
        )

    valid_bitmap = ran.get_valid_bitmap()

    err, valid_point_count = Epipolar_Constraint_Error(
        pp, valid_bitmap, focalLen_Cam0, focalLen_Cam1, F
    )
    print(f"Epipolar Constraint error = {err}, valid point count = {valid_point_count}")

    t, R = Fundamental_to_Trans_Rot(
        F, focalLen_Cam0, focalLen_Cam1, f0, pp, valid_bitmap
    )
    print(f"trans={t}\nrot={R}")
    PLY_Export_TwoCam(result_two_cam_ply, t, R)
    if result_cam_trans_rot_csv is not None:
        CSV_Write_CamPose(result_cam_trans_rot_csv, t, R)

    # Fから取得したt, R, focalLen_camを用いて、Fを再構築するテスト。
    reconF = Reconstruct_F_from(t, R, f0, focalLen_Cam0, focalLen_Cam1)
    print(f"reconstructedF={reconF}")

    P0, P1 = Build_two_cam_P0_P1(f0, focalLen_Cam0, focalLen_Cam1, t, R)

    AdjustTwoPoints(pp, valid_bitmap, theta, f0)

    xyz_list, valid_bitmap, valid_point_count = Triangulation(
        pp, valid_bitmap, f0, P0, P1
    )
    print(f"Triangulation valid_point_count={valid_point_count}")

    PLY_Export_PointList(
        result_3dpoints_ply, InlierPointList_from_bitmap(xyz_list, valid_bitmap)
    )
    return True


if __name__ == "__main__":

    parser = argparse.ArgumentParser(
        description="reads two cam feature point list csv, performs two cam RANSAC to write 3d point list PLY"
    )

    parser.add_argument(
        "--matched_point2d_csv",
        type=str,
        help="CSV file contains xy coordinates of matched 2d point pair",
        default="tmp/op0001_0002.csv",
    )
    parser.add_argument(
        "--result_3dpoints_ply",
        type=str,
        help="PLY file to write 3d point list",
        default="tmp/Run_TwoCam_Ransac_PointList.ply",
    )
    parser.add_argument(
        "--result_two_cam_ply",
        type=str,
        help="PLY file to contain camera pose",
        default="tmp/Run_TwoCam_Ransac_TwoCameraPoses.ply",
    )
    parser.add_argument(
        "--result_two_cam_pose_csv",
        type=str,
        help="CSV file to contain camera pose",
        default="tmp/Run_TwoCam_Ransac_TwoCameraPoses.csv",
    )
    parser.add_argument(
        "--result_two_cam_focal_len_csv",
        type=str,
        help="PLY file to contain two cam focal length",
        default=None,
    )
    parser.add_argument(
        "--regressor",
        type=str,
        choices=["lsq", "fns"],
        default="fns",
        help="regressor to use for two-cam pose estimation (lsq or fns)",
    )
    parser.add_argument(
        "--ite_count", type=int, default=1000, help="RANSAC maximum iterations"
    )
    parser.add_argument(
        "--loss_threshold",
        type=float,
        default=5.0,
        help="サンプソン誤差J threshold",
    )
    parser.add_argument(
        "--close_points_ratio",
        type=float,
        default=0.8,
        help="RANSACでインライアーと判定されるべき点の数の比率",
    )
    args = parser.parse_args()

    b = Run_TwoCam_Ransac(
        args.matched_point2d_csv,
        args.result_two_cam_ply,
        args.result_3dpoints_ply,
        args.result_two_cam_focal_len_csv,
        args.result_two_cam_pose_csv,
        args.regressor,
        args.ite_count,
        args.loss_threshold,
        args.close_points_ratio,
    )

    if b == True:
        rv = 0
    else:
        rv = 1
    sys.exit(rv)
