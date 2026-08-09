# https://docs.opencv.org/3.4/dc/dc3/tutorial_py_matcher.html

# commandline examples
# python Run_FeatureMatch.py --img1 cylinder_img/0001.png --img2 cylinder_img/0002.png --result_csv tmp/0001_0002.csv

import argparse
import os
from Common import *
import numpy as np
import cv2 as cv
import matplotlib.pyplot as plt


def Show_MatchedPoints(img1, kp1, img2, kp2, good, matchesMask, M, save_path):
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
    h,w = img1.shape
    pts = np.float32([ [0,0],[0,h-1],[w-1,h-1],[w-1,0] ]).reshape(-1,1,2)
    dst = cv.perspectiveTransform(pts, M)
    img2 = cv.polylines(img2,[np.int32(dst)],True,255,3, cv.LINE_AA)
    draw_params = dict(matchColor = (0,255,0), # draw matches in green color
                       singlePointColor = None,
                       matchesMask = matchesMask, # draw only inliers
                       flags = 2)
    img3 = cv.drawMatches(img1,kp1,img2,kp2,good,None,**draw_params)
    os.makedirs(os.path.dirname(save_path), exist_ok=True)
    plt.imsave(save_path, img3)


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
    for m,n in matches:
        if m.distance < ratio*n.distance:
            good.append(m)
            lratios.append(m.distance / n.distance)
    return good, lratios


def SIFT_FLANN2(img1, img2, ratio=0.7, ransac_threshold=5.0, save_png=None):
    """
    2枚の画像からSIFT特徴点を抽出し、FLANNで対応点を求めます。
    Lowe's ratio test と RANSAC(ホモグラフィ)で外れ値を除去した後、
    インライアの対応点ペアの座標リストを返します。

    引数:
        img1: 1枚目の画像 (グレースケール)
        img2: 2枚目の画像 (グレースケール)
        ratio: Lowe's ratio test のしきい値 (大きいほど候補が増える)
        ransac_threshold: ホモグラフィRANSACの再投影誤差しきい値 (大きいほど候補が増える)
        save_png: 指定すると、対応点の可視化画像をこのパスにPNG保存する

    戻り値:
        xy_pair: インライアの対応点ペアのリスト
                 [[ (x1,y1), (x2,y2), ratio, reproj_error ], ...]
                 各要素には Lowe's ratio 値と再投影誤差(pixel)を含む
    """
    sift = cv.SIFT_create()
    kp1, des1 = sift.detectAndCompute(img1,None)
    kp2, des2 = sift.detectAndCompute(img2,None)
    FLANN_INDEX_KDTREE = 1
    index_params = dict(algorithm = FLANN_INDEX_KDTREE, trees = 5)
    search_params = dict(checks = 50)
    flann = cv.FlannBasedMatcher(index_params, search_params)
    matches = flann.knnMatch(des1,des2,k=2)

    good, lratios = Extract_GoodMatches(matches, ratio)

    src_pts = np.float32([ kp1[m.queryIdx].pt for m in good ]).reshape(-1,1,2)
    dst_pts = np.float32([ kp2[m.trainIdx].pt for m in good ]).reshape(-1,1,2)

    M, mask = cv.findHomography(src_pts, dst_pts, cv.RANSAC, ransac_threshold)

    print(f"Homography={M}")

    matchesMask = mask.ravel().tolist()

    # インライア点のホモグラフィ再投影誤差 (pixel) を計算
    reproj_errors = None
    if len(src_pts) >= 4:
        proj_pts = cv.perspectiveTransform(src_pts, M)
        reproj_errors = np.linalg.norm(proj_pts - dst_pts, axis=2).ravel()

    if save_png is not None:
        Show_MatchedPoints(img1, kp1, img2, kp2, good, matchesMask, M, save_png)

    xy_pair=[]
    for i, m in enumerate(good):
        #print(f"i={i} m={m} mm={matchesMask[i]}")
        if matchesMask[i] != 0:
            #print(f"kp1 {kp1[m.queryIdx].pt} kp2 {kp2[m.trainIdx].pt}")
            err = float(reproj_errors[i]) if reproj_errors is not None else float('nan')
            xy_pair.append([ kp1[m.queryIdx].pt, kp2[m.trainIdx].pt, lratios[i], err ])
    return xy_pair



def CSV_Write_MatchedPointList(path, xy_pair, img1_shape):
    """
    対応点ペアリストをCSVファイルに書き出します。
    画素座標を、画像中心を原点とし幅・高さで正規化した座標系
    (x+→, y+↑) に変換して保存します。

    引数:
        path: 出力CSVファイルのパス
        xy_pair: 対応点ペアのリスト
                 [[ (x1,y1), (x2,y2), ratio, reproj_error ], ...]
        img1_shape: 1枚目の画像の形状 (高さ, 幅)

    出力形式: 1行に "x1, y1, x2, y2, ratio, reproj_error" を1対応点として書き出す
    """
    h,w = img1_shape

    w2 = w/2
    h2 = h/2

    # 座標系は、x+→, y+↑
    with open(path, 'w', newline='\n') as f: 
        for p in xy_pair:
            p0 = p[0]
            p1 = p[1]
            p0 = ( (p0[0] - w2) / w2, -(p0[1] - h2) / h2 )
            p1 = ( (p1[0] - w2) / w2, -(p1[1] - h2) / h2 )

            f.write(f"{p0[0]}, {p0[1]}, {p1[0]}, {p1[1]}, {p[2]}, {p[3]}\n")


def Run_SIFT_FLANN(args):
    """
    2枚の画像を読み込み、SIFT+FLANNで対応点を求め、CSV (と可視化PNG) を出力します。

    引数:
        args: コマンドライン引数 (img1, img2, result_csv, ratio, ransac_threshold, png_path)
    """
    img1 = cv.imread(args.img1, cv.IMREAD_GRAYSCALE) # queryImage
    img2 = cv.imread(args.img2, cv.IMREAD_GRAYSCALE) # trainImage

    img1_shape = img1.shape
    xy_pair = SIFT_FLANN2(img1, img2, args.ratio, args.ransac_threshold, save_png=args.png_path)

    print(f"xy_pair count={len(xy_pair)} (ratio={args.ratio}, ransac_threshold={args.ransac_threshold})")

    print("confidence list (x1, y1, x2, y2, lratio, reproj_error_px):")
    for p in xy_pair:
        print(f"  ({p[0][0]:.1f}, {p[0][1]:.1f}) -> ({p[1][0]:.1f}, {p[1][1]:.1f})  ratio={p[2]:.3f}  reproj={p[3]:.3f}")

    # 出力先ディレクトリを作成し、CSVを出力します。
    os.makedirs(os.path.dirname(args.result_csv), exist_ok=True)
    CSV_Write_MatchedPointList(args.result_csv, xy_pair, img1_shape)

    #draw_params = dict(matchColor = (0,255,0),
    #                   singlePointColor = (255,0,0),
    #                   matchesMask = matchesMask,
    #                   flags = cv.DrawMatchesFlags_DEFAULT)
    #img3 = cv.drawMatchesKnn(img1,kp1,img2,kp2,matches,None,**draw_params)
    #plt.imshow(img3,),plt.show()


def Run_ORB_BruteForce(args):
    """
    ORB特徴点を Brute-Force マッチャーで対応付けし、CSVを出力します。
    (SIFT_FLANN とは別の方式のため、主にデバッグ用)

    引数:
        args: コマンドライン引数 (img1, img2, result_csv)
    """
    img1 = cv.imread(args.img1, cv.IMREAD_GRAYSCALE) # queryImage
    img2 = cv.imread(args.img2, cv.IMREAD_GRAYSCALE) # trainImage
    img1_shape = img1.shape

    orb = cv.ORB_create()

    kp1, des1 = orb.detectAndCompute(img1,None)
    kp2, des2 = orb.detectAndCompute(img2,None)

    bf = cv.BFMatcher(cv.NORM_HAMMING, crossCheck=True)

    matches = bf.match(des1,des2)

    matches = sorted(matches, key = lambda x:x.distance)
    N = len( matches )
    print(f"{args.img1} {args.img2} N={N}")

    xy_pair = []
    for m in matches:
        #print(f"m queryIdx={m.queryIdx}, trainIdx={m.trainIdx} dist={m.distance}")
        #print(f"kp1 {kp1[m.queryIdx].pt} kp2 {kp2[m.trainIdx].pt}")
        xy_pair.append([ kp1[m.queryIdx].pt, kp2[m.trainIdx].pt ])

    CSV_Write_MatchedPointList(args.result_csv, xy_pair, img1_shape)

    #img3 = cv.drawMatches(img1,kp1,img2,kp2,matches,None,flags=cv.DrawMatchesFlags_NOT_DRAW_SINGLE_POINTS)
    #plt.imshow(img3),plt.show()




if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='reads two images, write two cam feature point list csv')
    parser.add_argument('--img1',       type=str, help='image 1 png file')
    parser.add_argument('--img2',       type=str, help='image 2 png file')
    parser.add_argument('--result_csv', type=str, default='tmp/0001_0002.csv', help='CSV file to write two cam feature point list')
    parser.add_argument('--ratio',      type=float, default=0.7, help='Lowe ratio test threshold (0.7 default, larger = more candidates)')
    parser.add_argument('--ransac_threshold', type=float, default=5.0, help='RANSAC homography reprojection threshold (larger = more candidates)')
    args = parser.parse_args()

    base, _ = os.path.splitext(args.result_csv)
    args.png_path = base + '.png'

    # Run_ORB_BruteForce(args)
    Run_SIFT_FLANN(args)





