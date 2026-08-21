"""
2枚の画像から2カメラ間の相対姿勢 (t, R) と3D点群を推定します。

処理内容:
1. (Run_FeatureMatch) SIFT+FLANN で2枚の画像の対応点ペアを作成する
2. Lowe's ratio test とホモグラフィRANSACで外れ値を除去し、対応点CSVに書き出す
3. (Run_TwoCam_Ransac) 対応点CSVを読み込み、Ransac_TwoCam でファンダメンタル行列Fを求める
4. Fから相対姿勢 (t, R)、焦点距離、三角測量による3D点群を出力する

実行例:
    python Run_EstimateTwoCamPose.py --img1 Synthetic_OctPrism/0001.png --img2 Synthetic_OctPrism/0002.png

注意:
- 平行移動 t は長さ1に正規化されるため、絶対スケールは不定です。
"""

import argparse
import os

from Run_TwoCam_Ransac import Run_TwoCam_Ransac
from Run_FeatureMatch import Run_FeatureMatch


def Run_EstimateTwoCamPose(
    img1_path,
    img2_path,
    result_matched_points2d_csv,
    result_two_cam_ply,
    result_cam_trans_rot_csv,
    result_points3d_ply,
    result_two_cam_focal_len_csv,
    Lowes_ratio,
    homography_ransac_threshold,
    regressor,
    ite_count,
    loss_threshold,
    close_points_ratio,
    f0,
):
    base, _ = os.path.splitext(result_matched_points2d_csv)
    result_matched_points2d_png = base + ".png"

    # ---- 1. 対応点抽出 (SIFT + FLANN) ----
    b = Run_FeatureMatch(
        img1_path,
        img2_path,
        result_matched_points2d_csv,
        Lowes_ratio,
        homography_ransac_threshold,
        result_matched_points2d_png,
    )
    if b is not True:
        print(f"Run_FeatureMatch failed.")
        return False

    # ---- 2. RANSAC で基礎行列Fを求める ----
    b = Run_TwoCam_Ransac(
        result_matched_points2d_csv,
        result_two_cam_ply,
        result_points3d_ply,
        result_two_cam_focal_len_csv,
        result_cam_trans_rot_csv,
        regressor,
        ite_count,
        loss_threshold,
        close_points_ratio,
        f0,
    )

    return b


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="estimate two-cam pose from two images (SIFT+FLANN -> RANSAC)"
    )
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
        "--result_matched_points2d_csv",
        type=str,
        default="tmp/op0001_0002.csv",
        help="CSV file to write two cam feature point list",
    )
    parser.add_argument(
        "--result_points3d_ply",
        type=str,
        default="tmp/ResultPoints3D.ply",
        help="PLY file to write 3d point list",
    )
    parser.add_argument(
        "--result_second_cam_csv",
        type=str,
        default="tmp/ResultSecondCamPose.csv",
        help="CSV file to contain second camera pose t R",
    )
    parser.add_argument(
        "--result_two_cam_focal_len_csv",
        type=str,
        default="tmp/ResultTwoCamFocalLengths.csv",
        help="CSV file to contain estimated two cam focal lengths",
    )
    parser.add_argument(
        "--result_two_cam_ply",
        type=str,
        default="tmp/ResultTwoCamEst2r.ply",
        help="PLY file to contain camera pose",
    )
    parser.add_argument(
        "--result_cam_trans_rot_csv",
        type=str,
        default="tmp/ResultCamTransRot.csv",
        help="CSV file to contain cam pose",
    )
    parser.add_argument(
        "--Lowes_ratio", type=float, default=0.7, help="Lowe's ratio test threshold"
    )
    parser.add_argument(
        "--homography_ransac_threshold",
        type=float,
        default=20.0,
        help="homography RANSAC homography reprojection threshold",
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

    parser.add_argument(
        "--regressor",
        type=str,
        choices=["lsq", "fns"],
        default="fns",
        help="regressor to use for two-cam pose estimation (lsq or fns)",
    )
    parser.add_argument(
        "--f0",
        type=float,
        default=600,
        help="f0 parameter. feature point spread in px.",
    )
    args = parser.parse_args()

    b = Run_EstimateTwoCamPose(
        args.img1,
        args.img2,
        args.result_matched_points2d_csv,
        args.result_two_cam_ply,
        args.result_cam_trans_rot_csv,
        args.result_points3d_ply,
        args.result_two_cam_focal_len_csv,
        args.Lowes_ratio,
        args.homography_ransac_threshold,
        args.regressor,
        args.ite_count,
        args.loss_threshold,
        args.close_points_ratio,
        args.f0,
    )
    if b is not True:
        raise SystemExit(1)
