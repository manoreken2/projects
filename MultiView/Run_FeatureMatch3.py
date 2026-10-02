# https://docs.opencv.org/3.4/dc/dc3/tutorial_py_matcher.html

# commandline examples
# python Run_FeatureMatch3.py --img1 Synthetic_OctPrism/0001.png --img2 Synthetic_OctPrism/0002.png --img3 Synthetic_OctPrism/0003.png --cam1id=1, --cam2id=2, --cam3id=3 --result_csv 01_02_03.csv

import argparse
from Common import *
import numpy as np
import cv2 as cv
import os


def Extract_GoodMatches(matches, ratio):
    good = []
    for m, n in matches:
        if m.distance < ratio * n.distance:
            good.append(m)
    return good


def Find_QueryIdx(matches, idx):
    for i, m in enumerate(matches):
        if m.queryIdx == idx:
            return m
    return None


def Ransac_Homography_Mask(src_pts, dst_pts, ransac_threshold):
    # 参考: Run_FeatureMatch.py の SIFT_FLANN2 と同様の外れ値除去
    M, mask = cv.findHomography(src_pts, dst_pts, cv.RANSAC, ransac_threshold)
    if M is None or mask is None:
        # ホモグラフィ推定失敗: 全点を外れ値扱い(トリプレットは作られない)
        print("Ransac_Homography_Mask: findHomography failed")
        return [0] * len(src_pts)
    return mask.ravel().tolist()


def SIFT_FLANN3(img1, img2, img3, ratio=0.7, ransac_threshold=35.0):
    sift = cv.SIFT_create()
    kp1, des1 = sift.detectAndCompute(img1, None)
    kp2, des2 = sift.detectAndCompute(img2, None)
    kp3, des3 = sift.detectAndCompute(img3, None)

    FLANN_INDEX_KDTREE = 1
    index_params = dict(algorithm=FLANN_INDEX_KDTREE, trees=5)
    search_params = dict(checks=50)
    flann = cv.FlannBasedMatcher(index_params, search_params)

    matches12 = flann.knnMatch(des1, des2, k=2)
    matches23 = flann.knnMatch(des2, des3, k=2)

    good12 = Extract_GoodMatches(matches12, ratio)
    good23 = Extract_GoodMatches(matches23, ratio)

    if len(good12) >= 4 and len(good23) >= 4:
        src12 = np.float32([kp1[m.queryIdx].pt for m in good12]).reshape(-1, 1, 2)
        dst12 = np.float32([kp2[m.trainIdx].pt for m in good12]).reshape(-1, 1, 2)
        mask12 = Ransac_Homography_Mask(src12, dst12, ransac_threshold)

        src23 = np.float32([kp2[m.queryIdx].pt for m in good23]).reshape(-1, 1, 2)
        dst23 = np.float32([kp3[m.trainIdx].pt for m in good23]).reshape(-1, 1, 2)
        mask23 = Ransac_Homography_Mask(src23, dst23, ransac_threshold)
    else:
        mask12 = [1] * len(good12)
        mask23 = [1] * len(good23)

    xyz_triplet = []
    for i, m in enumerate(good12):
        if mask12[i] == 0:
            continue
        m23 = Find_QueryIdx(good23, m.trainIdx)
        if m23 != None:
            j = good23.index(m23)
            if mask23[j] == 0:
                continue
            # print(
            #    f"kp1 {kp1[m.queryIdx].pt} kp2 {kp2[m.trainIdx].pt} kp3{kp3[m23.trainIdx].pt}"
            # )
            xyz_triplet.append(
                [kp1[m.queryIdx].pt, kp2[m.trainIdx].pt, kp3[m23.trainIdx].pt]
            )
    return xyz_triplet


def Run_FeatureMatch3(
    img1_path,
    cam1_id,
    img2_path,
    cam2_id,
    img3_path,
    cam3_id,
    result_csv,
    lowes_ratio,
    ransac_threshold,
):
    img1 = cv.imread(img1_path, cv.IMREAD_GRAYSCALE)
    img2 = cv.imread(img2_path, cv.IMREAD_GRAYSCALE)
    img3 = cv.imread(img3_path, cv.IMREAD_GRAYSCALE)

    img1_shape = img1.shape
    xyz_triplet = SIFT_FLANN3(img1, img2, img3, lowes_ratio, ransac_threshold)

    print(
        f"triplet count={len(xyz_triplet)} (Lowe's ratio={lowes_ratio}, ransac_threshold={ransac_threshold})"
    )

    if len(xyz_triplet) < 8:
        print("  skip: not enough 3-cam feature point triplets")
        return False

    # CSVを出力します。
    shift_xy = np.array([-img1_shape[1] * 0.5, -img1_shape[0] * 0.5])
    CSV_Write_MatchedPointList3(
        result_csv, xyz_triplet, shift_xy, [cam1_id, cam2_id, cam3_id]
    )
    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="reads two images, write two cam feature point list csv"
    )
    parser.add_argument(
        "--img1",
        type=str,
        help="image 1 png file",
        default="Synthetic_OctPrism/0001.png",
    )
    parser.add_argument(
        "--cam1id",
        type=int,
        default=1,
        help="image 1 camera id",
    )
    parser.add_argument(
        "--img2",
        type=str,
        help="image 2 png file",
        default="Synthetic_OctPrism/0002.png",
    )
    parser.add_argument(
        "--cam2id",
        type=int,
        default=2,
        help="image 2 camera id",
    )
    parser.add_argument(
        "--img3",
        type=str,
        help="image 3 png file",
        default="Synthetic_OctPrism/0003.png",
    )
    parser.add_argument(
        "--cam3id",
        type=int,
        default=3,
        help="image 3 camera id",
    )
    parser.add_argument(
        "--result_feature_point_list_csv",
        type=str,
        help="CSV file to write 3 cam feature point list",
        default="tmp/op0001_0002_0003.csv",
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
    args = parser.parse_args()

    Run_FeatureMatch3(
        args.img1,
        args.cam1id,
        args.img2,
        args.cam2id,
        args.img3,
        args.cam3id,
        args.result_feature_point_list_csv,
        args.lowes_ratio,
        args.ransac_threshold,
    )
