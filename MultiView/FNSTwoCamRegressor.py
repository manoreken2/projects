from Common import BuildXi_F, BuildV0_F, BuildM, BuildL
from Ransac import Ransac, RegresserBase
import numpy as np
from numpy.linalg import eigh

class FNSTwoCamRegressor(RegresserBase):
    def __init__(self, MaxIter, ConvEPS, f0):
        self.ev = None
        self.MaxIter = MaxIter
        self.ConvEPS = ConvEPS
        self.f0 = f0
        self.theta = None
        self.w_list = None

    def get_theta(self):
        return self.theta
    
    def get_w_list(self):
        return self.w_list

    def fit(self, x_list: np.ndarray, y_list: np.ndarray):
        self.theta, self.w_list = self.FNS(x_list, y_list)
        return self

    # N個のロス値を戻します。
    def calc_loss(self, x0_list, y0_list, x1_list, y1_list):
        N = x0_list.shape[0]
        assert N == y0_list.shape[0]
        assert N == x1_list.shape[0]
        assert N == y1_list.shape[0]

        theta = self.theta
        xi_list = BuildXi_F(x0_list, y0_list, x1_list, y1_list, self.f0)
        v0_list = BuildV0_F(x0_list, y0_list, x1_list, y1_list, self.f0)

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

    def FNS(self, x_list: np.ndarray, y_list: np.ndarray):
        MaxIter = self.MaxIter
        ConvEPS = self.ConvEPS
        f0 = self.f0

        N = x_list.shape[0]
        assert N == y_list.shape[0]
        
        xi_list = BuildXi(x_list, y_list, f0)
        v0_list = BuildV0(x_list, y_list, f0)

        # w_list: N個の1が入っている。
        w_list = np.asarray(N * [1.0])

        theta0=np.vstack(np.zeros(6))
        for i in range(MaxIter):
            M = BuildM(xi_list, w_list)
            L = BuildL(xi_list, w_list, v0_list, theta0)

            X = M - L

            # Xの固有ベクトルthetaを得る。
            _, eig_vec=eigh(X)
            theta = eig_vec[:, 0].reshape(6)
            theta = np.vstack(theta)

            # スケールしても式の内容は変わらない。定数項が-1になるようにする。
            theta /= -theta[5, 0]

            diff = np.linalg.norm(theta0 - theta)
            if diff < ConvEPS:
                break
            
            for al in range(N):
                v0   = v0_list[al]
                v0ev = np.matmul(v0, theta) # matrix mul vector
                w_list[al] = 1.0 / (theta.T @ v0ev).item() # 1x1 mat to number

            theta0 = theta

        #print(f'Iteration {i}, ev: {theta}')
        return theta, w_list
