# 手順5.3 基礎行列Fから焦点距離、運動パラメータ算出。
import numpy as np
from numpy.linalg import eigh, svd, det
import math
from Common import *
import scipy.linalg as scipy_linalg
import pytest


# 行列Mの最小の固有値に対応する固有ベクトルを得る。
def MinEigVec(M):
    _, u = eigh(M)
    ev = u[:, 0]
    ev = np.vstack(ev)
    return ev


sqeuclidean = lambda x: np.vdot(x, x)


# 基礎行列Fからカメラ0焦点距離f、カメラ1焦点距離fpを得る。
def Fundamental_to_FocalLength(F, f0):
    k = np.array([[0], [0], [1]])

    Ft = np.transpose(F)
    FFt = F @ Ft
    FtF = Ft @ F
    e = MinEigVec(FFt)
    ep = MinEigVec(FtF)

    Fk = F @ k
    Ftk = Ft @ k
    FFtFk = F @ Ft @ Fk
    k_dot_FFtFk = np.vdot(k, FFtFk)

    e_cross_k_se = sqeuclidean(np.cross(np.transpose(e), np.transpose(k)))
    ep_cross_k_se = sqeuclidean(np.cross(np.transpose(ep), np.transpose(k)))

    k_dot_Fk = np.vdot(k, Fk)
    k_dot_Fk_sq = k_dot_Fk * k_dot_Fk

    Fk_se = sqeuclidean(Fk)
    Ftk_se = sqeuclidean(Ftk)

    numerator_xi = Fk_se - k_dot_FFtFk * ep_cross_k_se / k_dot_Fk
    numerator_eta = Ftk_se - k_dot_FFtFk * e_cross_k_se / k_dot_Fk

    xi = numerator_xi / (ep_cross_k_se * Ftk_se - k_dot_Fk_sq)

    eta = numerator_eta / (e_cross_k_se * Fk_se - k_dot_Fk_sq)

    if xi <= -1:
        raise ValueError("imaginary focal point problem occured on xi")
    if eta <= -1:
        raise ValueError("imaginary focal point problem occured on eta")

    f = f0 / math.sqrt(1.0 + xi)
    fp = f0 / math.sqrt(1.0 + eta)

    return f, fp


def Scalar_triplet(a, b, c):
    return np.vdot(a, np.cross(np.transpose(b), np.transpose(c)))


# 3次元ベクトルa → 3x3行列ax (5.2章)
def a_to_ax(a):
    a = np.asarray(a).ravel()  # 1次元、または 3x1 列ベクトルを受け付ける
    a1 = np.float64(a[0])
    a2 = np.float64(a[1])
    a3 = np.float64(a[2])

    # 書いてある通りにメモリに並ぶ。
    ax = np.array([[0, -a3, a2], [a3, 0, -a1], [-a2, a1, 0]])
    return ax


# 基礎行列F, カメラ0焦点距離f, カメラ1焦点距離fp, カメラ0点列、カメラ1点列から平行移動t, 回転行列Rを求める。
def Fundamental_to_Trans_Rot(F, f, fp, f0, pp: Point2dPair, valid_bitmap):
    N = pp.get_point_count()

    FF = np.eye(3)
    FF[0, 0] = 1.0 / f0
    FF[1, 1] = 1.0 / f0
    FF[2, 2] = 1.0 / f

    FFp = np.eye(3)
    FFp[0, 0] = 1.0 / f0
    FFp[1, 1] = 1.0 / f0
    FFp[2, 2] = 1.0 / fp

    # Essential行列E
    E = FF @ F @ FFp

    # 平行移動の方向t (長さはわからないので 1)
    t = MinEigVec(E @ np.transpose(E))

    # t の向き・R をシーン基準(両カメラの前方に点がある)で決める。
    # 注意: F(したがってE)の全体符号は推定により任意なので、旧来の
    # Σ[t, x, E x'] テストは E に埋め込まれた t の符号と t を揃えるだけで、
    # シーンに対して逆向きを選ぶことがある(厳密Fでも t→-t になる実測バグ)。
    # 候補 (±t, ±E) の4組合せについて R を再構成し、三角測量で両カメラ
    # 前方(Z>0)の点数が最大の組合せを選ぶ(H&Z の4候補判定と同趣旨)。
    def candidate(tv, Es):
        txv = a_to_ax(tv)
        Kv = -txv @ (Es * E)
        srv = svd(Kv)
        Dv = np.eye(3)
        Dv[2, 2] = det(srv.U @ srv.Vh)
        Rv = srv.U @ Dv @ srv.Vh
        Rt = Rv.T
        t1 = -Rt @ tv.reshape(3)  # P1 = [R^T | t1], カメラ1中心 = tv
        cnt = 0
        step = max(1, N // 200)
        for i in range(0, N, step):
            if valid_bitmap[i] == False:
                continue
            x0 = pp.a[i, 0]
            y0 = pp.a[i, 1]
            x1 = pp.b[i, 0]
            y1 = pp.b[i, 1]
            # P0=[I|0] 規約(正規化座標 x=(x,y,f0))での三角測量
            A4 = np.array(
                [
                    [f0, 0.0, -x0],
                    [0.0, f0, -y0],
                    f0 * Rt[0, :] - x1 * Rt[2, :],
                    f0 * Rt[1, :] - y1 * Rt[2, :],
                ]
            )
            b4 = np.array(
                [
                    0.0,
                    0.0,
                    -(f0 * t1[0] - x1 * t1[2]),
                    -(f0 * t1[1] - y1 * t1[2]),
                ]
            )
            X = np.linalg.lstsq(A4, b4, rcond=None)[0]
            if X[2] <= 0:
                continue
            if (Rt @ (X - tv.reshape(3)))[2] > 0:
                cnt += 1
        return cnt, Rv

    best = None
    for tv in (t, -t):
        for Es in (1.0, -1.0):
            cnt, Rv = candidate(tv, Es)
            if best is None or cnt > best[0]:
                best = (cnt, tv, Rv)
    t = best[1]
    R = best[2]

    return t, R


def Build_two_cam_P0_P1(f0, fl0, fl1, t, R):
    """
    p.78 eq 5.7
    """

    P0 = np.array([[fl0, 0, 0, 0], [0, fl0, 0, 0], [0, 0, f0, 0]])

    RT = np.transpose(R)
    RTt = RT @ t

    RT_trans = np.concatenate((RT, -RTt), axis=1)

    P1 = np.array([[fl1, 0, 0], [0, fl1, 0], [0, 0, f0]]) @ RT_trans

    return P0, P1


if __name__ == "__main__":
    # テスト
    F = CSV_Read_F("Chap5_GroundTruth_F.csv")

    DEFAULT_F0 = 600
    f, fp = Fundamental_to_FocalLength(F, DEFAULT_F0)
    assert pytest.approx(f) == 846.937
    assert pytest.approx(fp) == 849.379

    print(f"TEST PASSED")
