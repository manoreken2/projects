# 3章 手順3.8 RANSAC 2つの画像の対応点から基礎行列Fを求める。
# 実行例
# python Run_TwoCam_Ransac.py --matched_point2d_csv "twoCamPoints410_outlier10.csv" --result_ply "ResultPoints3D.ply" --result_second_cam_csv "ResultSecondCamPose.csv" --result_two_cam_ply "ResultTwoCamEst2r.ply" --result_two_cam_focallengths_csv="ResultTwoCamFocalLengths.csv"
# python Run_TwoCam_Ransac.py --matched_point2d_csv 0001_0002.csv --result_ply Result_0001_0002.ply --result_second_cam_csv Result_0001_0002_SecondCam.csv --result_two_cam_ply Result_0001_0002_Est.ply --result_two_cam_focallengths_csv=Result_0001_0002_FocalLengths.csv

import sys
import argparse
from Common import *
from Rank_Correction import Rank_Correction
from Fundamental_to_CamParams import *
from Ransac_TwoCam import *
from RegressorTwoCamFNS import RegressorTwoCamFNS
from mpl_toolkits import mplot3d
import numpy as np
import matplotlib.pyplot as plt


def main(args):
    f0=1

    pp = CSV_Read_TwoCamPointList(args.matched_point2d_csv)
    N = pp.get_point_count()

    ran = Ransac_TwoCam(f0=f0, close_points=N//2, ite_count=300, loss_threshold=1e-6, model=RegressorTwoCamFNS())
    rv = ran.fit(pp)
    if rv == None:
        return "RANSAC failed."

    theta = ran.get_theta()
    F = ThetaToF(theta)
    print(f"F={F}")

    fl0 = 1.0
    fl1 = 1.0
    #fl0, fl1 = Fundamental_to_FocalLength(F, f0)

    print(f"Focal length = {fl0}, {fl1}")
    CSV_Write_TwoCamFocalLengths(args.result_two_cam_focallengths_csv, fl0, fl1)

    valid_bitmap  = ran.get_valid_bitmap()
    picked_up_ids = ran.get_picked_up_ids()

    #PlotValidPoints(pp, valid_bitmap);

    err = Epipolar_Constraint_Error(pp, valid_bitmap, f0, F)
    print(f"Epipolar Constraint error = {err}")

    t, R = Fundamental_to_Trans_Rot(F, fl0, fl1, f0, pp, valid_bitmap)
    CSV_Write_CamPose(args.result_second_cam_csv, t, R)

    PLY_Export_TwoCam(t, R, args.result_two_cam_ply)

    P0, P1 = TwoCamMat(f0, fl0, fl1, t, R)

    AdjustTwoPoints(pp, valid_bitmap, theta, f0)

    xyz_list, valid_bitmap = Triangulation(pp, valid_bitmap, f0, P0, P1)

    #Plot3D(xyz_list, new_loss_list)

    PLY_Export_PointList(InlierPointList_from_bitmap(xyz_list, valid_bitmap), args.result_ply)
    #PLY_Export_PointList(InlierPointList_from_inlierIdList(xyz_list, picked_up_ids), "ResultKernelPoints3D.ply")
    return 0

if __name__ == "__main__":
    # args example: 
    # --matched_point2d_csv             "twoCamPoints410_outlier10.csv" 
    # --result_ply                      "ResultPoints3D.ply"
    # --result_second_cam_csv           "ResultSecondCamPose.csv"
    # --result_two_cam_ply              "ResultTwoCamEst2r.ply"
    # --result_two_cam_focallengths_csv "ResultTwoCamFocalLengths.csv"

    parser = argparse.ArgumentParser(
        description='reads two cam feature point list csv, performs two cam RANSAC to write 3d point list PLY')

    parser.add_argument('--matched_point2d_csv',             type=str, help='CSV file contains xy coordinates of matched 2d point pair')
    parser.add_argument('--result_ply',                      type=str, help='PLY file to write 3d point list')
    parser.add_argument('--result_second_cam_csv',           type=str, help='CSV file to contain second camera pose t R')
    parser.add_argument('--result_two_cam_focallengths_csv', type=str, help='CSV file to contain estimated two cam focal lengths')
    parser.add_argument('--result_two_cam_ply',              type=str, help='PLY file to contain camera pose')
    args = parser.parse_args()

    rv = main(args)
    sys.exit(rv)


   
