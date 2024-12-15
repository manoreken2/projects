import matplotlib.pyplot as plt
import csv
import numpy as np
from numpy.linalg import eigh
import math
from Common import ReadTwoCamPoints, Plot, BuildXi_F, BuildV0_F, BuildM_LS, BuildL, Epipolar_Constraint_Error, ThetaToF
from Ransac import Ransac, RegresserBase
from FNSTwoCamRegressor import FNSTwoCamRegressor


# 最小二乗法で最適解を求めます。
def TwoCam_LeastSquare(x0_list, y0_list, x1_list, y1_list, f0):
    xi_list = BuildXi_F(x0_list, y0_list, x1_list, y1_list, f0)
    M = BuildM_LS(xi_list)
    # Mの最小固有値に対応する固有ベクトルthetaを得る。
    _, eig_vec=eigh(M)
    theta = eig_vec[:, 0].reshape(9)
    theta = np.vstack(theta)

    # スケールしても式の内容は変わらない。定数項が-1になるようにする。
    theta /= -theta[8, 0]
    return theta

def main():
    f0=1
    MaxIter=100
    ConvEPS=1e-5
    SampleCount=9 # smallest point count to solve

    x0_list, y0_list, x1_list, y1_list = ReadTwoCamPoints('twoCamPoints.csv')
    N=x0_list.shape[0]
    assert N == y0_list.shape[0]
    assert N == x1_list.shape[0]
    assert N == y1_list.shape[0]

    theta = TwoCam_LeastSquare(x0_list, y0_list, x1_list, y1_list, f0)
    #print(theta)
    F = ThetaToF(theta)
    print(f"F={F}")
    err = Epipolar_Constraint_Error(x0_list, y0_list, x1_list, y1_list, f0, F)
    print(f"Epipolar constraint error = {err}")


if __name__ == "__main__":
    main()




