import argparse
from Common import *
import numpy as np
import cv2 as cv
import os
import sys


def build_observe_mat_W(fp_list, z_ak_mat):
    """
    p.201 eq.13.12
    観測行列W作成。
    """
    nPoints = len(fp_list)
    nCams = fp_list[0].CameraCount()

    #            行の数,    列の数
    W = np.zeros((3 * nCams, nPoints))

    for a in range(nPoints):
        fp = fp_list[a]
        for k in range(nCams):
            x_ak = fp.x_ak(k)
            z_ak = z_ak_mat[a, k]
            W[3 * k + 0 : 3 * k + 3, a : a + 1] = x_ak * z_ak

    return W


def normalize_vec(v):
    return v / np.linalg.norm(v)


def normalize_observe_mat(W):
    """
    観測行列Wの各列を単位ベクトルに正規化する。
    """
    nPoints = W.shape[1]

    newW = W.copy()
    for p in range(nPoints):
        newW[:, p] = normalize_vec(W[:, p])

    return newW


def build_Aalpha(fp: FeaturePoint, U):
    """
    特徴点fpに関する行列Aalphaを作る。
    p.202 eq.13.13
    """
    nCams = fp.CameraCount()

    A = np.zeros((nCams, nCams))

    for k in range(nCams):
        x_ak = fp.x_ak(k)
        norm_x_ak = np.linalg.norm(x_ak)
        for L in range(nCams):
            x_aL = fp.x_ak(L)
            norm_x_aL = np.linalg.norm(x_aL)

            scale = 1.0 / (norm_x_ak * norm_x_aL)

            s = 0.0
            for i in range(4):
                u_ik = U[3 * k : 3 * k + 3, i : i + 1]
                u_iL = U[3 * L : 3 * L + 3, i : i + 1]
                s += np.vdot(x_ak, u_ik) * np.vdot(x_aL, u_iL) * scale

            A[k, L] = s
    return A


class MultiCamSelfCalib:
    def __init__(self, f0):
        self.f0 = f0

    def calc_reproj_err(self, fp_list, P_list, X_list):
        """
        p.202 eq.13.16
        再投影誤差Eを求める。
        """

        f0 = self.f0

        nPoints = len(fp_list)
        nCams = fp_list[0].CameraCount()

        s = 0.0
        for a in range(nPoints):
            fp = fp_list[a]
            Xa = X_list[a]
            for k in range(nCams):
                Pk = P_list[k]

                PkXa = Pk @ Xa
                Z_PkXa = PkXa * (1.0 / PkXa[2])

                x_ak = fp.x_ak(k)

                x_ak_minus_Z_PkXa = x_ak - Z_PkXa

                s += np.vdot(x_ak_minus_Z_PkXa, x_ak_minus_Z_PkXa)

        e = f0 * np.sqrt(s * (1.0 / (nPoints * nCams)))
        return e

    def PrimaryMethod(self, fp_list, reproj_err_converge_diff):
        """
        基本法。
        p.202 手順13.2

        reproj_err_converge_diff: 再投影誤差改善量の打ち切り値(pixel)

        nPoints: 特徴点数
        nCams: カメラ数

        """
        nPoints = len(fp_list)
        nCams = fp_list[0].CameraCount()

        z_ak_mat = np.ones((nPoints, nCams))

        # P: カメラ行列 3x4
        # X: 特徴点の3次元座標の同次座標 4x1
        P_list = []
        X_list = []

        prev_reproj_err = sys.float_info.max
        for loop in range(100):
            W = build_observe_mat_W(fp_list, z_ak_mat)
            W = normalize_observe_mat(W)

            U_, Sigma, Vh_ = np.linalg.svd(W, full_matrices=False)

            # Uは、3Mx4 行列。
            U = U_[:, 0:4]

            for a in range(nPoints):
                fp = fp_list[a]
                A = build_Aalpha(fp, U)

                # Aの最大固有値に対する単位固有ベクトルxi_a
                # np.linalg.eighは、固有値を小さい順に戻す。
                # ■■ eighのeigvec_listは、縦ベクトルの固有ベクトルを横に並べた形で戻す
                # eigvec_listの最も右の列が、最大固有値に対応する固有ベクトル。
                _, eigvec_list = np.linalg.eigh(A)
                xi_a = eigvec_list[:, -1].flatten()

                # xi_aの符号を選ぶ。
                if np.sum(xi_a) < 0:
                    xi_a = -xi_a

                # xi_aの大きさを1にする。
                xi_a = normalize_vec(xi_a)

                # 射影的奥行き更新。
                for k in range(nCams):
                    z = xi_a[k] / np.linalg.norm(fp.x_ak(k))
                    z_ak_mat[a, k] = z

            # p.201 eq.13.10
            M = U

            # Sは、4xN行列。
            SigmaVh = np.diag(Sigma) @ Vh_
            S = SigmaVh[0:4, :]

            # P: カメラ行列
            # X: 3次元点座標

            P_list = []
            for k in range(nCams):
                P = M[3 * k : 3 * k + 3, :]
                P_list.append(P)

            X_list = []
            for a in range(nPoints):
                X = S[0:4, a]
                X = X.reshape(4, 1)
                X_list.append(X)

            reproj_err = self.calc_reproj_err(fp_list, P_list, X_list)
            print(
                f"PrimaryMethod reproj_err={reproj_err}, thr={reproj_err_converge_diff}"
            )
            if np.abs(prev_reproj_err - reproj_err) < reproj_err_converge_diff:
                return P_list, X_list
            prev_reproj_err = reproj_err
        return None


def Run_MultiCamSelfCalib(csv_path, f0, reproj_err_threshold):
    fp_list = CSV_Read_FeaturePointList(csv_path, f0)

    sc = MultiCamSelfCalib(f0)
    P_list, _ = sc.PrimaryMethod(fp_list, reproj_err_threshold)

    print(f"P_list=\n{P_list}")

    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="reads Run_FeatureMatch3 result csv, estimate cam pose"
    )

    parser.add_argument(
        "--multicam_csv",
        type=str,
        help="input csv file",
        default="tmp/0000_0001_0002.csv",
    )

    parser.add_argument(
        "--f0",
        type=float,
        help="f0 param of csv file. Typically it is image size in pixel.",
        default=600,
    )
    parser.add_argument(
        "--reproj_err_converge",
        type=float,
        help="reprojection error converge iteration diff in pixel.",
        default=0.01,
    )
    args = parser.parse_args()

    br = Run_MultiCamSelfCalib(args.multicam_csv, args.f0, args.reproj_err_converge)
    if br == True:
        rv = 0
    else:
        rv = 1
    sys.exit(rv)
