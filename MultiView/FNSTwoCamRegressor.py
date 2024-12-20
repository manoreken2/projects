from Common import *
from Rank_Correction import Rank_Correction
from Fundamental_to_CamParams import Fundamental_to_Trans_Rot, Fundamental_to_FocalLength
from Ransac_TwoCam import Point2dPair, RegresserBase_TwoCam
import numpy as np
from numpy.linalg import eigh

class FNSTwoCamRegressor(RegresserBase_TwoCam):
    def __init__(self, MaxIter=100, ConvEPS=0.01, f0=1.0):
        self.ev = None
        self.MaxIter = MaxIter
        self.ConvEPS = ConvEPS
        self.f0 = f0
        self.theta = None

    def get_theta(self):
        return self.theta
    
    def fit_tc(self, pp: Point2dPair):
        MaxIter = self.MaxIter
        ConvEPS = self.ConvEPS
        f0      = self.f0

        N = pp.get_point_count()
    
        theta = TwoCam_FNS(pp, f0, ConvEPS, MaxIter)
        theta = Rank_Correction(theta, pp, f0)
        self.theta = theta

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
            #print("theta=",theta)
            #print("v0theta=", v0_theta)

            thetaT_v0_theta = (theta.T @ v0_theta).item()

            J[i] = xi_theta ** 2 / (thetaT_v0_theta)

        #print(f"J={J}")

        return J




