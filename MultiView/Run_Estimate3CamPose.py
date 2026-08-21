"""
3枚の画像から3カメラ間の相対姿勢 (t, R) と3D点群を推定します。

"""

import argparse
import os

from Run_FeatureMatch3 import Run_FeatureMatch3
from Run_MultiCamSelfCalib import Run_MultiCamSelfCalib


def Run_Estimate3CamPose(
    img1_path,
    cam1_id,
    img2_path,
    cam2_id,
    img3_path,
    cam3_id,
    result_feature_points_list_csv,
    result_cam_pose_csv,
    result_cam_pose_ply,
    result_points3d_csv,
    result_points3d_ply,
    Lowes_ratio,
    homography_ransac_threshold,
    reproj_err_converge,
    j_threshold,
    f0=-1,
    method="dual",
    shared_intrinsic=False,
):
    """
    f0: f0 parameter. if it is negative number, f0 is calculated from feature point spread.
    """
    b = Run_FeatureMatch3(
        img1_path,
        cam1_id,
        img2_path,
        cam2_id,
        img3_path,
        cam3_id,
        result_feature_points_list_csv,
        Lowes_ratio,
        homography_ransac_threshold,
    )
    if b is not True:
        print("Run_Estimate3CamPose Run_FeatureMatch3 failed.")
        return False

    b = Run_MultiCamSelfCalib(
        result_feature_points_list_csv,
        result_cam_pose_csv,
        result_cam_pose_ply,
        result_points3d_csv,
        result_points3d_ply,
        reproj_err_converge,
        j_threshold,
        f0,
        [cam1_id, cam2_id, cam3_id],
        method,
        shared_intrinsic,
    )

    return b


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="estimate 3-cam pose from 3 images")
    parser.add_argument(
        "--img1",
        type=str,
        help="image 1 png file",
        default="Synthetic_OctPrism/0001.png",
    )
    parser.add_argument(
        "--img2",
        type=str,
        help="image 2 png file",
        default="Synthetic_OctPrism/0002.png",
    )
    parser.add_argument(
        "--img3",
        type=str,
        help="image 3 png file",
        default="Synthetic_OctPrism/0003.png",
    )
    parser.add_argument(
        "--cam1_id",
        type=int,
        default=1,
        help="image 1 camera id",
    )
    parser.add_argument(
        "--cam2_id",
        type=int,
        default=2,
        help="image 2 camera id",
    )
    parser.add_argument(
        "--cam3_id",
        type=int,
        default=3,
        help="image 3 camera id",
    )
    parser.add_argument(
        "--result_feature_points_list_csv",
        type=str,
        help="CSV file to write 3 cam feature point (x,y) list",
        default="tmp/result_feature_points2d_0001_0002_0003.csv",
    )
    parser.add_argument(
        "--result_cam_pose_csv",
        type=str,
        help="output camera pose CSV file",
        default="tmp/result_campose_0000_0001_0002.csv",
    )
    parser.add_argument(
        "--result_cam_pose_ply",
        type=str,
        help="output camera pose PLY file",
        default="tmp/result_campose_0000_0001_0002.ply",
    )
    parser.add_argument(
        "--result_points3d_csv",
        type=str,
        help="output 3d points CSV file",
        default="tmp/result_points3d_0000_0001_0002.csv",
    )
    parser.add_argument(
        "--result_points3d_ply",
        type=str,
        help="output 3d points PLY file",
        default="tmp/result_points3d_0000_0001_0002.ply",
    )
    parser.add_argument(
        "--lowes_ratio",
        type=float,
        default=0.7,
        help="Lowe ratio test threshold (0.7 default, larger = more candidates)",
    )
    parser.add_argument(
        "--ransac_threshold",
        type=float,
        default=35.0,
        help="RANSAC homography reprojection threshold (larger = more candidates)",
    )
    parser.add_argument(
        "--reproj_err_converge",
        type=float,
        help="reprojection error converge iteration diff in pixel.",
        default=0.001,
    )
    parser.add_argument(
        "--j_threshold",
        type=float,
        help="Euclidean upgrade threshold in pixel.",
        default=1.0,
    )
    parser.add_argument(
        "--f0",
        type=float,
        help="f0 parameter in pixel. typically 600 to 800. When unspecified, optimal value is calculated from feature point spread.",
        default=-1.0,
    )
    parser.add_argument(
        "--method",
        type=str,
        choices=["primary", "dual"],
        help="perspective self calibration method.",
        default="dual",
    )
    parser.add_argument(
        "--shared_intrinsic",
        action="store_true",
        help="constrain all cameras to share one common intrinsic K "
        "(same physical camera assumption)",
    )
    args = parser.parse_args()

    b = Run_Estimate3CamPose(
        args.img1,
        args.cam1_id,
        args.img2,
        args.cam2_id,
        args.img3,
        args.cam3_id,
        args.result_feature_points_list_csv,
        args.result_cam_pose_csv,
        args.result_cam_pose_ply,
        args.result_points3d_csv,
        args.result_points3d_ply,
        args.lowes_ratio,
        args.ransac_threshold,
        args.reproj_err_converge,
        args.j_threshold,
        args.f0,
        args.method,
        args.shared_intrinsic,
    )
    if b is not True:
        raise SystemExit(1)
