# https://docs.opencv.org/3.4/dc/dc3/tutorial_py_matcher.html

# commandline examples
# python Run_FeatureMatch.py --img1 cylinder_img/0001.png --img2 cylinder_img/0002.png --result_csv 01_02.csv

import argparse
from Common import *
from Rank_Correction import Rank_Correction
from Fundamental_to_CamParams import *
from Ransac_TwoCam import *
from RegressorLSQTwoCam import RegressorLSQTwoCam
from RegressorFNSTwoCam import RegressorFNSTwoCam
from mpl_toolkits import mplot3d
import numpy as np
import matplotlib.pyplot as plt


import numpy as np
import cv2 as cv
import matplotlib.pyplot as plt

def SIFT_FLANN2(img1, img2):
    sift = cv.SIFT_create()
    kp1, des1 = sift.detectAndCompute(img1,None)
    kp2, des2 = sift.detectAndCompute(img2,None)
    FLANN_INDEX_KDTREE = 1
    index_params = dict(algorithm = FLANN_INDEX_KDTREE, trees = 5)
    search_params = dict(checks = 50)
    flann = cv.FlannBasedMatcher(index_params, search_params)
    matches = flann.knnMatch(des1,des2,k=2)

    good = []
    for m,n in matches:
        if m.distance < 0.7*n.distance:
            good.append(m)

    src_pts = np.float32([ kp1[m.queryIdx].pt for m in good ]).reshape(-1,1,2)
    dst_pts = np.float32([ kp2[m.trainIdx].pt for m in good ]).reshape(-1,1,2)

    M, mask = cv.findHomography(src_pts, dst_pts, cv.RANSAC,5.0)

    print(f"Homography={M}")

    matchesMask = mask.ravel().tolist()
    #h,w = img1.shape
    #pts = np.float32([ [0,0],[0,h-1],[w-1,h-1],[w-1,0] ]).reshape(-1,1,2)
    #dst = cv.perspectiveTransform(pts,M)
    #img2 = cv.polylines(img2,[np.int32(dst)],True,255,3, cv.LINE_AA)
    #draw_params = dict(matchColor = (0,255,0), # draw matches in green color
    #                   singlePointColor = None,
    #                   matchesMask = matchesMask, # draw only inliers
    #                   flags = 2)
    #img3 = cv.drawMatches(img1,kp1,img2,kp2,good,None,**draw_params)
    #plt.imshow(img3, 'gray'),plt.show()

    xy_pair=[]
    for i, m in enumerate(good):
        #print(f"i={i} m={m} mm={matchesMask[i]}")
        if matchesMask[i] != 0:
            #print(f"kp1 {kp1[m.queryIdx].pt} kp2 {kp2[m.trainIdx].pt}")
            xy_pair.append([ kp1[m.queryIdx].pt, kp2[m.trainIdx].pt ])
    return xy_pair



def CSV_Write_MatchedPointList(path, xy_pair, img1_shape):
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

            f.write(f"{p0[0]}, {p0[1]}, {p1[0]}, {p1[1]}\n")


def SIFT_FLANN(args):
    img1 = cv.imread(args.img1, cv.IMREAD_GRAYSCALE) # queryImage
    img2 = cv.imread(args.img2, cv.IMREAD_GRAYSCALE) # trainImage

    img1_shape = img1.shape
    xy_pair = SIFT_FLANN2(img1, img2)

    # CSVを出力します。
    CSV_Write_MatchedPointList(args.result_csv, xy_pair, img1_shape)

    #draw_params = dict(matchColor = (0,255,0),
    #                   singlePointColor = (255,0,0),
    #                   matchesMask = matchesMask,
    #                   flags = cv.DrawMatchesFlags_DEFAULT)
    #img3 = cv.drawMatchesKnn(img1,kp1,img2,kp2,matches,None,**draw_params)
    #plt.imshow(img3,),plt.show()


def ORB_BruteForce(args):
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
    parser.add_argument('--result_csv', type=str, help='CSV file to write two cam feature point list')
    args = parser.parse_args()

    #ORB_BruteForce(args)
    SIFT_FLANN(args)





