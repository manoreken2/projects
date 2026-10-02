# https://docs.opencv.org/3.4/dc/dc3/tutorial_py_matcher.html

# commandline examples
# python Run_FeatureMatch.py --img1 BlenderScene_OctPrism/0001.png --img2 BlenderScene_OctPrism/0002.png --result_csv tmp/0001_0002.csv

import argparse
import os
from Common import *
import numpy as np
import cv2 as cv
import matplotlib.pyplot as plt


def Draw_MatchedPoints(
    img1, kp1, img2, kp2, good, matchesMask, M, matchedpoints_img_path
):
    """
    マッチした特徴点の可視化画像をPNGとして保存します。
    2枚の画像を横に並べ、RANSACで求めたホモグラフィの矩形と、
    インライア(外れ値でない)の対応点のみを線で結んで描画します。

    引数:
        img1: 1枚目の画像 (グレースケール)
        kp1 : 1枚目の特徴点リスト
        img2: 2枚目の画像 (グレースケール)
        kp2 : 2枚目の特徴点リスト
        good: 抽出された対応点のマッチリスト
        matchesMask: 各対応点がインライアかどうかを表す0/1のリスト
        M   : ホモグラフィ行列 (cv.findHomography の出力)
        save_path: 保存先PNGファイルのパス
    """
    h, w = img1.shape
    pts = np.float32([[0, 0], [0, h - 1], [w - 1, h - 1], [w - 1, 0]]).reshape(-1, 1, 2)
    dst = cv.perspectiveTransform(pts, M)
    img2 = cv.polylines(img2, [np.int32(dst)], True, 255, 3, cv.LINE_AA)
    draw_params = dict(
        matchColor=(0, 255, 0),  # draw matches in green color
        singlePointColor=None,
        matchesMask=matchesMask,  # draw only inliers
        flags=2,
    )
    img3 = cv.drawMatches(img1, kp1, img2, kp2, good, None, **draw_params)
    out_dir = os.path.dirname(matchedpoints_img_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    plt.imsave(matchedpoints_img_path, img3)


def Extract_GoodMatches(matches, ratio):
    """
    Lowe's ratio test により良好なマッチのみを抽出します。
    最良のマッチ距離が2番目のマッチ距離の ratio 倍未満のものを採用します。

    引数:
        matches: flann.knnMatch の出力 (各要素は (m, n) の2つのマッチ)
        ratio  : 採用判定のしきい値 (小さいほど厳しく絞り込む)

    戻り値:
        good: 条件を満たしたマッチのリスト
        lratios: good の各マッチに対応する Lowe's ratio 値 (m.distance / n.distance) のリスト
    """
    good = []
    lratios = []
    for m, n in matches:
        if m.distance < ratio * n.distance:
            good.append(m)
            lratios.append(m.distance / n.distance)
    return good, lratios


def SIFT_FLANN2(img1, img2, Lowes_ratio=0.7, ransac_threshold=5.0, save_png=None):
    """
    2枚の画像からSIFT特徴点を抽出し、FLANNで対応点を求めます。
    Lowe's ratio test で絞り込み、さらに必要に応じて RANSAC(ホモグラフィ)で
    外れ値を除去した後、対応点ペアの座標リストを返します。

    引数:
        img1: 1枚目の画像 (グレースケール)
        img2: 2枚目の画像 (グレースケール)
        Lowes_ratio: Lowe's ratio test のしきい値 (大きいほど候補が増える)
        ransac_threshold: ホモグラフィRANSACの再投影誤差しきい値 (大きいほど候補が増える)
        save_png: 指定すると、対応点の可視化画像をこのパスにPNG保存する

    戻り値:
        xy_pair: インライアの対応点ペアのリスト
                 [[ (x1,y1), (x2,y2), ratio, reproj_error ], ...]
                 各要素には Lowe's ratio 値と再投影誤差(pixel)を含む。
    """
    sift = cv.SIFT_create()
    kp1, des1 = sift.detectAndCompute(img1, None)
    kp2, des2 = sift.detectAndCompute(img2, None)
    FLANN_INDEX_KDTREE = 1
    index_params = dict(algorithm=FLANN_INDEX_KDTREE, trees=5)
    search_params = dict(checks=50)
    flann = cv.FlannBasedMatcher(index_params, search_params)
    matches = flann.knnMatch(des1, des2, k=2)

    good, lratios = Extract_GoodMatches(matches, Lowes_ratio)

    if len(good) < 4:
        print(f"SIFT_FLANN2: too few matches for findHomography ({len(good)} < 4)")
        return []

    src_pts = np.float32([kp1[m.queryIdx].pt for m in good]).reshape(-1, 1, 2)
    dst_pts = np.float32([kp2[m.trainIdx].pt for m in good]).reshape(-1, 1, 2)

    M, mask = cv.findHomography(src_pts, dst_pts, cv.RANSAC, ransac_threshold)
    print(f"Homography={M}")
    if M is None or mask is None:
        print("SIFT_FLANN2: findHomography failed (no inlier homography)")
        return []
    matchesMask = mask.ravel().tolist()

    # インライア点のホモグラフィ再投影誤差 (pixel) を計算
    reproj_errors = None
    if len(src_pts) >= 4:
        proj_pts = cv.perspectiveTransform(src_pts, M)
        reproj_errors = np.linalg.norm(proj_pts - dst_pts, axis=2).ravel()

    if save_png is not None:
        Draw_MatchedPoints(img1, kp1, img2, kp2, good, matchesMask, M, save_png)

    xy_pair = []
    for i, m in enumerate(good):
        # print(f"i={i} m={m} mm={matchesMask[i]}")
        if matchesMask[i] != 0:
            # print(f"kp1 {kp1[m.queryIdx].pt} kp2 {kp2[m.trainIdx].pt}")
            err = float(reproj_errors[i]) if reproj_errors is not None else float("nan")
            xy_pair.append([kp1[m.queryIdx].pt, kp2[m.trainIdx].pt, lratios[i], err])

    # 多重マッチの重複除去:
    # 各対応点ペアを信頼度の高い順 (再投影誤差小 / Lowe's ratio 小) に並べ、
    # 同一の画像1側キーポイントと画像2側キーポイントが重複しないように1対1対応のみ残す
    # (reproj_errors が無いときの NaN は最後尾に置く)
    xy_pair.sort(key=lambda p: p[3] if np.isfinite(p[3]) else np.inf)

    seen_q = set()
    seen_t = set()
    unique = []
    for p in xy_pair:
        q = tuple(p[0])
        t = tuple(p[1])
        if q in seen_q or t in seen_t:
            continue
        seen_q.add(q)
        seen_t.add(t)
        unique.append(p)
    return unique


def Run_FeatureMatch(
    img1_path,
    img2_path,
    result_matchedpoints_csv_path,
    Lowes_ratio,
    homography_ransac_threshold,
    result_matchedpoints_img_path,
):
    """
    2枚の画像を読み込み、SIFT+FLANNで対応点を求め、CSV (と可視化PNG) を出力します。

    引数:
        args: コマンドライン引数 (img1, img2, result_csv, ratio, ransac_threshold, png_path)

    戻り値:
        True: 成功
        False: 失敗
    """
    img1 = cv.imread(img1_path, cv.IMREAD_GRAYSCALE)  # queryImage
    img2 = cv.imread(img2_path, cv.IMREAD_GRAYSCALE)  # trainImage

    img1_Width = img1.shape[1]
    img1_Height = img1.shape[0]

    xy_pair = SIFT_FLANN2(
        img1,
        img2,
        Lowes_ratio,
        homography_ransac_threshold,
        result_matchedpoints_img_path,
    )
    print(
        f"xy_pair count={len(xy_pair)} (Lowes_ratio={Lowes_ratio}, homography_ransac_threshold={homography_ransac_threshold})"
    )

    if len(xy_pair) < 8:
        print("  skip: not enough feature matches")
        return False

    # 画像上のマッチ点座標を、(x,y) x右+, y下+で、画面中心を原点として保存。

    out_dir = os.path.dirname(result_matchedpoints_csv_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    shift_xy = np.array([-img1_Width * 0.5, -img1_Height * 0.5])
    CSV_Write_TwoCam_MatchedPointList(
        result_matchedpoints_csv_path,
        xy_pair,
        shift_xy,
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
        "--img2",
        type=str,
        help="image 2 png file",
        default="Synthetic_OctPrism/0002.png",
    )
    parser.add_argument(
        "--result_csv",
        type=str,
        default="tmp/op0001_0002.csv",
        help="CSV file to write two cam feature point list",
    )
    parser.add_argument(
        "--Lowes_ratio",
        type=float,
        default=0.7,
        help="Lowe's ratio test threshold (0.7 default, larger = more candidates)",
    )
    parser.add_argument(
        "--ransac_threshold",
        type=float,
        default=35.0,
        help="RANSAC homography reprojection threshold (larger = more candidates)",
    )
    args = parser.parse_args()

    base, _ = os.path.splitext(args.result_csv)
    args.png_path = base + ".png"

    Run_FeatureMatch(
        args.img1,
        args.img2,
        args.result_csv,
        args.Lowes_ratio,
        args.ransac_threshold,
        args.png_path,
    )
