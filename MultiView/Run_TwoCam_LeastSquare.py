# 3章 手順3.1 2つの画像の対応点から基礎行列Fを求める。

# python Run_TwoCam_LeastSquare.py --matched_point2d_csv 0001_0002.csv 

import argparse
from Common import *
from Rank_Correction import Rank_Correction
from Fundamental_to_CamParams import Fundamental_to_Trans_Rot, Fundamental_to_FocalLength


def main(args):
    f0=1

    pp = CSV_Read_TwoCamPointList(args.matched_point2d_csv)
    N = pp.get_point_count()
    
    theta = TwoCam_LeastSquare(pp, f0)
    print(f"theta={theta}")
    F = ThetaToF(theta)
    print(f"F={F}")

    theta = Rank_Correction(theta, pp, f0)
    print(f"rank correction theta={theta}")
    
    F = ThetaToF(theta)
    print(f"F={F}")

    valid_bitmap = N * [True]

    err = Epipolar_Constraint_Error(pp, valid_bitmap, f0, F)
    print(f"Epipolar Constraint error = {err}")

    #fl0, fl1 = Fundamental_to_FocalLength(F, 1.0)
    fl0 = fl1 = 0.12
    print(f"Focal length = {fl0} {fl1}")

    t, R = Fundamental_to_Trans_Rot(F, fl0, fl1, f0, pp, valid_bitmap)

    print(f"trans={t}\nrot={R}")

    PLY_Export_TwoCam(t, R, 'twoCamEst2.ply')

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='reads two cam feature point list csv, performs two cam least square')

    parser.add_argument('--matched_point2d_csv', type=str, help='CSV file contains xy coordinates of matched 2d point pair')
    args = parser.parse_args()

    main(args)




