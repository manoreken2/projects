# 手順5.3 基礎行列Fから焦点距離、運動パラメータ算出。
import numpy as np
from numpy.linalg import eigh, svd, det
import math
from Common import *

# 行列Mの最小の固有値に対応する固有ベクトルを得る。
def MinEigVec(M):
    _, u=eigh(M)
    ev = u[:, 0]
    ev = np.vstack(ev)
    return ev

sqeuclidean = lambda x: np.vdot(x, x)

# 基礎行列Fからカメラ0焦点距離f、カメラ1焦点距離fpを得る。
def Fundamental_to_FocalLength(F, f0):
    k = np.array([[0],[0],[1]])

    Ft = np.transpose(F)
    FFt = F @ Ft
    FtF = Ft @ F
    e  = MinEigVec(FFt)
    ep = MinEigVec(FtF)

    Fk = F @ k
    Ftk = Ft @ k
    FFtFk = F @ Ft @ Fk
    k_dot_FFtFk = np.vdot(k, FFtFk)

    e_cross_k_se  = sqeuclidean(np.cross(np.transpose(e),  np.transpose(k)))
    ep_cross_k_se = sqeuclidean(np.cross(np.transpose(ep), np.transpose(k)))

    k_dot_Fk = np.vdot(k, Fk)
    k_dot_Fk_sq = k_dot_Fk * k_dot_Fk

    Fk_se  = sqeuclidean(Fk)
    Ftk_se = sqeuclidean(Ft @ k)

    numerator_xi  = Fk_se  - k_dot_FFtFk * ep_cross_k_se / k_dot_Fk
    numerator_eta = Ftk_se - k_dot_FFtFk * e_cross_k_se  / k_dot_Fk

    xi = numerator_xi / \
        (ep_cross_k_se * Ftk_se - k_dot_Fk_sq)

    eta = numerator_eta / \
        (e_cross_k_se  * Fk_se  - k_dot_Fk_sq)

    if xi <= -1:
        raise ValueError('imaginary focal point problem occured on xi')
    if eta <= -1:
        raise ValueError('imaginary focal point problem occured on eta')

    f  = f0 / math.sqrt(1.0 + xi)
    fp = f0 / math.sqrt(1.0 + eta)

    return f, fp

def Scalar_triplet(a, b, c):
    return np.vdot(a, np.cross(np.transpose(b), np.transpose(c)))


# 3次元ベクトルa → 3x3行列ax (5.2章)
def a_to_ax(a):
    a1 = np.float64(a[0])
    a2 = np.float64(a[1])
    a3 = np.float64(a[2])

    # 書いてある通りにメモリに並ぶ。
    ax = np.array([      \
        [ 0,  -a3,  a2], \
        [ a3,  0,  -a1], \
        [-a2,  a1,  0]   \
        ])
    return ax

# 基礎行列F, カメラ0焦点距離f, カメラ1焦点距離fp, カメラ0点列、カメラ1点列から平行移動t, 回転行列Rを求める。
def Fundamental_to_Trans_Rot(F, f, fp, f0, pp: Point2dPair, valid_bitmap):
    N = pp.get_point_count()

    FF = np.eye(3)
    FF[0,0] = 1.0/f0
    FF[1,1] = 1.0/f0
    FF[2,2] = 1.0/f

    FFp = np.eye(3)
    FFp[0,0] = 1.0/f0
    FFp[1,1] = 1.0/f0
    FFp[2,2] = 1.0/fp

    # Essential行列E
    E = FF @ F @ FFp

    # 平行移動の方向t (長さはわからないので 1)
    t = MinEigVec(E @ np.transpose(E))

    # tの向きが反対かどうか調べる。
    s = 0.0
    for i in range(N):
        if valid_bitmap[i] == False:
            continue
        x0 = pp.a[i,0]
        y0 = pp.a[i,1]
        x1 = pp.b[i,0]
        y1 = pp.b[i,1]

        xa  = np.vstack([x0/f,  y0/f,  1.0])
        xap = np.vstack([x1/fp, y1/fp, 1.0])
        s += Scalar_triplet(t, xa, E @ xap)
    if s <= 0:
        t = -t

    tx = a_to_ax(t)
    K = -tx @ E

    sr = svd(K)

    #print(sr.S)

    U = sr.U
    Vh = sr.Vh

    D = np.eye(3)
    D[2,2] = det(U @ Vh)

    #print(f"K=\n{K}")
    #print(f"U=\n{U}")
    #print(f"D=\n{D}")
    #print(f"Vh=\n{Vh}")

    R = U @ D @ Vh

    #print(f"DVht=\n{D @ Vh}")

    #print(f"R=\n{R}")

    return t, R

def TwoCamMat(f0, fl0, fl1, t, R):
    P0=np.array([          \
        [fl0, 0,   0,  0], \
        [0,   fl0, 0,  0], \
        [0,   0,   f0, 0]])

    RT = np.transpose(R)
    RTt= RT @ t

    RTtt = np.concat((RT, RTt), axis=1)

    P1= np.array([     \
        [fl1, 0,   0], \
        [0,   fl1, 0], \
        [0,   0,   f0]]) @ RTtt

    return P0, P1
