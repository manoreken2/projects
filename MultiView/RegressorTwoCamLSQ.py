from Common import *
from Rank_Correction import Optimal_rank_correction
from Fundamental_to_CamParams import (
    Fundamental_to_Trans_Rot,
    Fundamental_to_FocalLength,
)
from Ransac_TwoCam import Point2dPair
from RegressorTwoCamBase import RegressorTwoCamBase
import numpy as np
from numpy.linalg import eigh


class RegressorTwoCamLSQ(RegressorTwoCamBase):
    def __init__(self, f0):
        self.ev = None
        self.f0 = f0
        self.theta = None

    def get_theta(self):
        return self.theta

    def get_F(self):
        return ThetaToF(self.theta)

    # def fit_tc(self, xy0_list: np.ndarray, xy1_list: np.ndarray):
    #    self.theta = self.LeastSquare(xy0_list=xy0_list, xy1_list=xy1_list)
    #    return self

    def fit_TwoCam(self, pp: Point2dPair):
        f0 = self.f0

        theta = TwoCam_LeastSquare(pp, f0)

        theta = Optimal_rank_correction(theta, pp, f0)
        self.theta = theta
        return theta

    # N個のロス値を戻します。
    def calc_loss_tc(self, pp: Point2dPair):
        N = pp.get_point_count()

        theta = self.theta
        xi_list = BuildXi_F(pp, self.f0)
        v0_list = BuildV0_F(pp, self.f0)

        # サンプソン誤差J
        J = np.zeros(N)
        for i in range(N):
            xi = xi_list[i]
            v0 = v0_list[i]

            xi_theta = (xi.T @ theta).item()

            v0_theta = v0 @ theta
            # print("theta=",theta)
            # print("v0theta=", v0_theta)

            thetaT_v0_theta = (theta.T @ v0_theta).item()

            # 分母が負のとき負のロスになり閾値判定を常に通過してしまう。絶対値を取る。
            J[i] = abs(xi_theta**2 / (thetaT_v0_theta))

        # print(f"J={J}")

        return J
