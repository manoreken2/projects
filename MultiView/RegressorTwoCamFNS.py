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


class RegressorTwoCamFNS(RegressorTwoCamBase):
    def __init__(self, MaxIter=100, ConvEPS=1e-05, f0=1.0):
        self.ev = None
        self.MaxIter = MaxIter
        self.ConvEPS = ConvEPS
        self.f0 = f0
        self.theta = None

    def get_theta(self):
        return self.theta

    def get_F(self):
        return ThetaToF(self.theta)

    def fit_TwoCam(self, pp: Point2dPair):
        MaxIter = self.MaxIter
        ConvEPS = self.ConvEPS
        f0 = self.f0

        N = pp.get_point_count()

        theta = TwoCam_FNS(pp, f0, ConvEPS, MaxIter)
        if theta is None:
            return None

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

            J[i] = abs(xi_theta**2 / (thetaT_v0_theta))

        # print(f"J={J}")

        return J


if __name__ == "__main__":
    # 動作テストする。
    f0 = 600.0
    pp = CSV_Read_TwoCam_MatchedPointList("Chap3_synthetic_2d_point_pairs.csv")
    xi_list = BuildXi_F(pp, f0)
    v0_list = BuildV0_F(pp, f0)

    # p.59 FNS法は、解から離れた位置から始めると収束しないので、
    # LSQ法よりもTaubin法で、初期値を与えたほうが収束性が向上。
    theta = TwoCam_Taubin(pp, f0)
    theta /= theta[8]

    print(f"Taubin theta={theta}")

    MaxIter = 30
    ConvEPS = 1e-05
    theta = TwoCam_FNS(pp, f0, ConvEPS, MaxIter)
    print(f"FNS Theta={theta}")
