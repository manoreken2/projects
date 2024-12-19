from Common import *
from Rank_Correction import Rank_Correction
from Fundamental_to_CamParams import Fundamental_to_Trans_Rot, Fundamental_to_FocalLength
from Ransac_TwoCam import *
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
    
    def fit_tc(self, xy0_list: np.ndarray, xy1_list: np.ndarray):
        self.theta = self.FNS(xy0_list=xy0_list, xy1_list=xy1_list)
        return self

    # N個のロス値を戻します。
    def calc_loss(self, xy0_list, xy1_list):
        N = xy0_list.shape[0]
        assert N == xy1_list.shape[0]

        theta = self.theta
        xi_list = BuildXi_F(xy0_list, xy1_list, self.f0)
        v0_list = BuildV0_F(xy0_list, xy1_list, self.f0)

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

    def FNS(self, xy0_list: np.ndarray, xy1_list: np.ndarray):
        MaxIter = self.MaxIter
        ConvEPS = self.ConvEPS
        f0 = self.f0

        N = xy0_list.shape[0]
        assert N == xy1_list.shape[0]
    
        theta = TwoCam_FNS(xy0_list, xy1_list, f0, ConvEPS, MaxIter)

        theta = Rank_Correction(theta, xy0_list, xy1_list, f0)
        #print(theta)
    
        #F = ThetaToF(theta)
        #print(f"F={F}")

        return theta



