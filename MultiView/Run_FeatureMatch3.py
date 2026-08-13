# https://docs.opencv.org/3.4/dc/dc3/tutorial_py_matcher.html

# commandline examples
# python Run_FeatureMatch3.py --img1 Synthetic_OctPrism/0001.png --img2 Synthetic_OctPrism/0002.png --img3 Synthetic_OctPrism/0003.png --result_csv 01_02_03.csv

import argparse
from Common import *
import numpy as np
import cv2 as cv


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
            print(
                f"kp1 {kp1[m.queryIdx].pt} kp2 {kp2[m.trainIdx].pt} kp3{kp3[m23.trainIdx].pt}"
            )
            xyz_triplet.append(
                [kp1[m.queryIdx].pt, kp2[m.trainIdx].pt, kp3[m23.trainIdx].pt]
            )
    return xyz_triplet


def CSV_Write_MatchedPointList3(path, xyz_triplet, shift_xy):
    sx = shift_xy[0]
    sy = shift_xy[1]

    # 座標系は、x+→, y+↓
    with open(path, "w", newline="\n") as f:
        f.write(f"x0, y0, x1, y1, x2, y2\n")
        for p in xyz_triplet:
            p0 = p[0]
            p1 = p[1]
            p2 = p[2]
            p0 = ((p0[0] + sx), (p0[1] + sy))
            p1 = ((p1[0] + sx), (p1[1] + sy))
            p2 = ((p2[0] + sx), (p2[1] + sy))

            f.write(f"{p0[0]}, {p0[1]}, {p1[0]}, {p1[1]}, {p2[0]}, {p2[1]}\n")


def Run_SIFT_FLANN(args):
    img1 = cv.imread(args.img1, cv.IMREAD_GRAYSCALE)
    img2 = cv.imread(args.img2, cv.IMREAD_GRAYSCALE)
    img3 = cv.imread(args.img3, cv.IMREAD_GRAYSCALE)

    img1_shape = img1.shape
    xyz_triplet = SIFT_FLANN3(img1, img2, img3, args.ratio, args.ransac_threshold)

    print(
        f"triplet count={len(xyz_triplet)} (ratio={args.ratio}, ransac_threshold={args.ransac_threshold})"
    )

    # CSVを出力します。
    shift_xy = np.array([-img1_shape[1] * 0.5, -img1_shape[0] * 0.5])
    CSV_Write_MatchedPointList3(args.result_csv, xyz_triplet, shift_xy)

    # draw_params = dict(matchColor = (0,255,0),
    #                   singlePointColor = (255,0,0),
    #                   matchesMask = matchesMask,
    #                   flags = cv.DrawMatchesFlags_DEFAULT)
    # img3 = cv.drawMatchesKnn(img1,kp1,img2,kp2,matches,None,**draw_params)
    # plt.imshow(img3,),plt.show()


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
        "--result_csv",
        type=str,
        help="CSV file to write two cam feature point list",
        default="tmp/op0001_0002_0003.csv",
    )
    parser.add_argument(
        "--ratio",
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

    Run_SIFT_FLANN(args)
