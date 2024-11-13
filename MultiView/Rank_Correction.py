# 手順3.4 最適ランク補正。
import numpy as np
from numpy.linalg import eigh
import math
from Common import ThetaDagger

def VectorScaleToUnitMagnitude(v):
    magn = np.linalg.norm(v)
    v = v / magn
    return v

def PTheta(theta):
    Pt = np.eye(9) - (theta @ np.transpose(theta))
    return Pt

def ThetaToMHat(theta, xi_list, V0_list):
    N = len(xi_list)
    Pt = PTheta(theta)
    MHat = np.zeros((9,9))

    for i in range(N):
        xi = xi_list[i]
        V0 = V0_list[i]
        PtXi = Pt @ xi
        denom = N * np.vdot(theta, V0 @ theta)
        mx = (PtXi @ np.transpose(PtXi)) * (1.0/denom)
        MHat = MHat + mx
    return MHat

def MHatToV0t(MHat, N):
    # 行列MHatの固有値のリストl_listと
    #           固有ベクトルのリストu_list
    l_list, u_list=eigh(MHat)

    # V0tを計算。
    V0t = np.eye(9)
    for i in range(9):
        l = l_list[i]
        u = u_list[:, i].reshape(9)
        u = VectorScaleToUnitMagnitude(u);

        #print(f"eVal={l}, eVec={u}")
        V0t = V0t + (u @ np.transpose(u))/(l * N)
    return V0t

def Rank_Correction(theta, xi_list, V0_list):
    N = len(xi_list)
    threshold = 1e-6
    iter_limit = 10

    MHat = ThetaToMHat(theta, xi_list, V0_list)
    V0t = MHatToV0t(MHat, N)
    
    for j in range(iter_limit):
        td = ThetaDagger(theta)
    
        # θを更新。
        theta = theta - np.vdot(td, theta) * (V0t @ td) / \
            (3.0 * np.vdot(td, (V0t @ td)))

        theta = VectorScaleToUnitMagnitude(theta)

        # Ptを更新。
        Pt = PTheta(theta)

        # V0tを更新。
        V0t = Pt @ V0t @ Pt

        d = abs(np.vdot(td, theta))
        if (d < threshold):
            return theta

