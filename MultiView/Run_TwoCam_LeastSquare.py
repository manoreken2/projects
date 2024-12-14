# 3章 手順3.1 2つの画像の対応点から基礎行列Fを求める。

from Common import *
from Rank_Correction import Rank_Correction
from Fundamental_to_CamParams import Fundamental_to_Trans_Rot, Fundamental_to_FocalLength


def main():
    f0=1

    xy0_list, xy1_list = CSV_Read_TwoCamPointList('twoCamPoints2.csv')

    N=xy0_list.shape[0]
    assert N == xy1_list.shape[0]
    
    theta = TwoCam_LeastSquare(xy0_list, xy1_list, f0)

    theta = Rank_Correction(theta, xy0_list, xy1_list,f0)
    print(theta)
    
    F = ThetaToF(theta)
    print(f"F={F}")

    err = Epipolar_Constraint_Error(xy0_list, xy1_list, f0, F)
    print(f"Epipolar Constraint error = {err}")

    fl0, fl1 = Fundamental_to_FocalLength(F, 1.0)
    print(f"Focal length = {fl0} {fl1}")

    t, R = Fundamental_to_Trans_Rot(F, fl0, fl1, f0, xy0_list, xy1_list)

    print(f"trans={t}\nrot={R}")

    PLY_Export_TwoCam(t, R, 'twoCamEst2.ply')

if __name__ == "__main__":
    main()




