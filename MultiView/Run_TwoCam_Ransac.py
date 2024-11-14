# 3章 手順3.8 RANSAC 2つの画像の対応点から基礎行列Fを求める。

import matplotlib.pyplot as plt
import csv
import numpy as np
from numpy.linalg import eigh
import math
from Common import *
from Ransac import Ransac, RegresserBase
from FNSTwoCamRegressor import FNSTwoCamRegressor
from Rank_Correction import Rank_Correction
from Fundamental_to_CamParams import Fundamental_to_Trans_Rot, FundamentalToFocalLength


# 最小二乗法で最適解を求めます。
def TwoCam_LeastSquare(x0_list, y0_list, x1_list, y1_list, f0):
    xi_list = BuildXi_F(x0_list, y0_list, x1_list, y1_list, f0)
    V0_list = BuildV0_F(x0_list, y0_list, x1_list, y1_list, f0)

    M = BuildM_LS(xi_list)
    # Mの最小固有値に対応する固有ベクトルthetaを得る。
    _, eig_vec=eigh(M)
    theta = eig_vec[:, 0].reshape(9)
    theta = np.vstack(theta)

    # 定数項が1になるようにする。
    theta /= theta[8, 0]

    theta = Rank_Correction(theta, xi_list, V0_list)

    theta /= theta[8,0]

    return theta

def main():
    f0=1
    #SampleCount=9 # smallest point count to solve

    x0_list, y0_list, x1_list, y1_list = \
            ReadTwoCamPoints('twoCamPoints2.csv')

    N=x0_list.shape[0]
    assert N == y0_list.shape[0]
    assert N == x1_list.shape[0]
    assert N == y1_list.shape[0]

    theta = TwoCam_LeastSquare(x0_list, y0_list, x1_list, y1_list, f0)
    print(theta)
    
    F = ThetaToF(theta)
    print(f"F={F}")

    err = Epipolar_Constraint_Error(x0_list, y0_list, x1_list, y1_list, f0, F)
    print(f"Epipolar Constraint error = {err}")

    fl0, fl1 = FundamentalToFocalLength(F, 1.0)
    print(f"Focal length = {fl0} {fl1}")

    t, R = Fundamental_to_Trans_Rot(F, fl0, fl1, f0, x0_list, y0_list, x1_list, y1_list)

    print(f"trans={t}\nrot={R}")

    GeneratePLY_TwoCamTransRotZP(t, R, 'twoCamEst2.ply')

if __name__ == "__main__":
    main()




