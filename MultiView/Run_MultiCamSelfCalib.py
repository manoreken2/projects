import argparse
from Common import *
import numpy as np
import cv2 as cv
import os
import sys
import statistics


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


def build_Calpha(fp: FeaturePoint, U):
    """
    特徴点fpに関する行列Cを作る。(faster法)
    primary_method.cc faster_primary_method 相当。

    Cは CamNum x 4 行列。
    C(kp, i) = (x_ak / |x_ak|)・u_ik
    """
    nCams = fp.CameraCount()

    C = np.zeros((nCams, 4))

    for k in range(nCams):
        x_ak = fp.x_ak(k)
        x_ak_nrm = np.linalg.norm(x_ak)
        x_ak_hat = x_ak / x_ak_nrm

        for i in range(4):
            u_ik = U[3 * k : 3 * k + 3, i]
            C[k, i] = np.vdot(x_ak_hat, u_ik)

    return C


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
        for loop in range(10000):
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

            # P: カメラ行列(射影変換の不定性を含む。ユークリッド復元必要)
            # X: 3次元点座標の同次座標(射影変換の不定性を含む)

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
                f"PrimaryMethod {loop} reproj_err={reproj_err}, thr={reproj_err_converge_diff}"
            )
            if np.abs(prev_reproj_err - reproj_err) < reproj_err_converge_diff:
                self.P_list = P_list
                self.X_list = X_list
                return P_list, X_list
            prev_reproj_err = reproj_err
        return None, None

    def PrimaryMethodFaster(self, fp_list, reproj_err_converge_diff):
        """
        基本法 (faster版)。
        primary_method.cc faster_primary_method 相当。

        射影深度z_akの更新を、originalの
        Aalpha(固有分解)の代わりに、C行列のSVDで行うことで高速化する。

        reproj_err_converge_diff: 再投影誤差改善量の打ち切り値(pixel)

        """
        nPoints = len(fp_list)
        nCams = fp_list[0].CameraCount()

        z_ak_mat = np.ones((nPoints, nCams))

        # P: カメラ行列 3x4
        # X: 特徴点の3次元座標の同次座標 4x1
        P_list = []
        X_list = []

        prev_reproj_err = sys.float_info.max
        for loop in range(10000):
            W = build_observe_mat_W(fp_list, z_ak_mat)
            W = normalize_observe_mat(W)

            U_, Sigma, Vh_ = np.linalg.svd(W, full_matrices=False)

            # Uは、3Mx4 行列。
            U = U_[:, 0:4]

            for a in range(nPoints):
                fp = fp_list[a]
                C = build_Calpha(fp, U)

                # CのSVD。特異値は降順なので、
                # 左特異ベクトルの第0列が最大特異値に対応。
                Uc, _, _ = np.linalg.svd(C, full_matrices=False)
                xi_a = Uc[:, 0]

                # xi_aの符号を選ぶ。
                if np.sum(xi_a) < 0:
                    xi_a = -xi_a

                # 射影的奥行き更新。
                # (左特異ベクトルは単位ベクトル。)
                for k in range(nCams):
                    z = xi_a[k] / np.linalg.norm(fp.x_ak(k))
                    z_ak_mat[a, k] = z

            # p.201 eq.13.10
            M = U

            # Sは、4xN行列。
            SigmaVh = np.diag(Sigma) @ Vh_
            S = SigmaVh[0:4, :]

            # P: カメラ行列(射影変換の不定性を含む。ユークリッド復元必要)
            # X: 3次元点座標の同次座標(射影変換の不定性を含む)

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
                f"PrimaryMethodFaster {loop} reproj_err={reproj_err}, thr={reproj_err_converge_diff}"
            )
            if np.abs(prev_reproj_err - reproj_err) < reproj_err_converge_diff:
                self.P_list = P_list
                self.X_list = X_list
                return P_list, X_list
            prev_reproj_err = reproj_err
        return None, None

    def build_initial_Kk_list(self, camFocalLen_list):
        Kk_list = []

        nCams = len(camFocalLen_list)
        f0 = self.f0

        for k in range(nCams):
            # Kk: カメラの内部パラメーター行列
            f_k = camFocalLen_list[k]
            Kk = np.array([[f_k, 0, 0], [0, f_k, 0], [0, 0, f0]])
            Kk_list.append(Kk)

        return Kk_list

    def build_Omega(self, omega):
        """
        p.211 eq.13.43
        """
        sq2 = np.sqrt(2.0)

        Omega = np.array(
            [
                [omega[0], omega[4] / sq2, omega[5] / sq2, omega[6] / sq2],
                [omega[4] / sq2, omega[1], omega[7] / sq2, omega[8] / sq2],
                [omega[5] / sq2, omega[7] / sq2, omega[2], omega[9] / sq2],
                [omega[6] / sq2, omega[8] / sq2, omega[9] / sq2, omega[3]],
            ]
        )
        return Omega

    def calc_Omega(self, Kk_list):
        """
        Ωを求める。
        p.209 §13.3.2
        特徴点の座標(x,y)は、画像の中心が原点になるよう平行移動してありprincipal_point=(0,0)。

        Kk: カメラの内部パラメーター行列。
        """
        P_list = self.P_list
        nCams = len(P_list)

        Qk_list = []
        for k in range(nCams):
            Kk = Kk_list[k]
            Pk = P_list[k]

            # 手順 13.4 eq.13.40
            Qk = np.linalg.inv(Kk) @ Pk
            Qk_list.append(Qk)

        # eq.13.41
        A_ = build_A_(Qk_list)

        # p.210 eq.13.42
        A = build_A(A_)

        eigval, eigvec = np.linalg.eigh(A)
        # omega: 最小固有値に対する固有ベクトル。(単位長さに正規化。)
        omega = eigvec[:, 0].flatten()
        omega /= np.linalg.norm(omega)

        Omega = self.build_Omega(omega)

        # Omegaの固有値を大きい順にs1,s2,s3,s4、
        # 対応固有ベクトルをo1,o2,o3,o4 (縦長)にセット。
        s_, o_ = np.linalg.eigh(Omega)
        s1 = s_[3]
        s2 = s_[2]
        s3 = s_[1]
        s4 = s_[0]
        o1 = np.vstack(o_[:, 3])
        o2 = np.vstack(o_[:, 2])
        o3 = np.vstack(o_[:, 1])
        o4 = np.vstack(o_[:, 0])

        # Omega2: p.211 eq.13.44
        # H: p.214 eq.13.59
        if s3 > 0:
            Omega2 = s1 * o1 @ o1.T + s2 * o2 @ o2.T + s3 * o3 @ o3.T
            H = np.concatenate(
                [np.sqrt(s1) * o1, np.sqrt(s2) * o2, np.sqrt(s3) * o3, o4], axis=1
            )
        elif s2 < 0:
            Omega2 = -s4 * o4 @ o4.T - s3 * o3 @ o3.T - s2 * o2 @ o2.T
            H = np.concatenate(
                [np.sqrt(-s4) * o4, np.sqrt(-s3) * o3, np.sqrt(-s2) * o2, o1], axis=1
            )
        else:
            raise RuntimeError(
                f"Calc_Omega unexpected sign sigma. sigma3={s3}, sigma2={s2}"
            )

        return Omega2, H, Qk_list

    def improve_Kk_list(self, Omega, Kk_list, Qk_list):
        """
        p.213 手順13.5 Kkの補正
        """

        nCams = len(Kk_list)

        better_Kk_list = []
        Jk_list = [sys.float_info.max] * nCams

        for k in range(nCams):
            Kk = Kk_list[k]
            Qk = Qk_list[k]

            # p.213 eq.13.52
            Qk_Omega_QkT = Qk @ Omega @ Qk.T

            ck11 = Qk_Omega_QkT[0, 0]
            # ck21 = Qk_Omega_QkT[1, 0]
            ck31 = Qk_Omega_QkT[2, 0]
            ck12 = Qk_Omega_QkT[0, 1]
            ck22 = Qk_Omega_QkT[1, 1]
            # ck32 = Qk_Omega_QkT[2, 1]
            ck13 = Qk_Omega_QkT[0, 2]
            ck23 = Qk_Omega_QkT[1, 2]
            ck33 = Qk_Omega_QkT[2, 2]

            # p.213 eq.13.53
            Fk = ((ck11 + ck22) / ck33) - ((ck13 / ck33) ** 2) - ((ck23 / ck33) ** 2)

            if ck33 <= 0 or Fk <= 0:
                # Kk修正不要。
                continue

            if False:
                # p.213 eq.13.54 光軸点の修正。
                du0k = ck13 / ck33
                dv0k = ck23 / ck33
            else:
                # 光軸点の修正はスキップ。
                du0k = 0
                dv0k = 0

            # p.213 eq.13.54 焦点距離fkの修正量。
            dfk = np.sqrt(0.5 * (((ck11 + ck22) / ck33) - (du0k**2) - (dv0k**2)))

            # p.213 eq.13.55
            dKk = np.array([[dfk, 0, du0k], [0, dfk, dv0k], [0, 0, 1]])

            # p.213 eq.13.56
            newKk = np.sqrt(ck33) * Kk @ dKk
            better_Kk_list.append(newKk)

            # p.215 eq.13.60
            Jk = (
                ((ck11 / ck33 - 1.0) ** 2)
                + ((ck22 / ck33 - 1.0) ** 2)
                + 2.0 * (ck12**2 + ck23**2 + ck31**2) / (ck33**2)
            )
            Jk_list[k] = Jk

        return better_Kk_list, Jk_list

    def Euclidean_upgrade(self, camFocalLen_list, J_threshold):
        """
        p.215 手順13.6
        """
        Jmed = sys.float_info.max
        Kk_list = self.build_initial_Kk_list(camFocalLen_list)

        prev_Jmed = Jmed

        while True:
            Omega, H, Qk_list = self.calc_Omega(Kk_list)

            Kk_list, Jk_list = self.improve_Kk_list(Omega, Kk_list, Qk_list)

            # p.215 eq.13.61
            Jmed = statistics.median(Jk_list)
            print(f"Euclidean_upgrade Jmed={Jmed}")
            if Jmed < J_threshold or Jmed >= prev_Jmed:
                # 終了条件達成。
                return H, Kk_list

            prev_Jmed = Jmed

    def build_X3d_list(self, X_list, H_inv):
        """
        p.216 eq.13.63
        """
        nPoints = len(X_list)

        X3d_list = []
        for a in range(nPoints):
            Xa = X_list[a]
            Xa = H_inv @ Xa

            # p.198 eq.13.2
            X3d = Xa[0:3, 0:1] / Xa[3, 0]
            X3d_list.append(X3d)

        return X3d_list

    def calc_sum_all_points_Z_sgn(self, X3d_list, RkT, tk):
        """
        ほとんどの点がカメラが見える方向になっているかどうかを調べる。
        座標をカメラ座標系に持って行ったとき、
        カメラが見えている方向はZ+であることを利用。
        """
        nPoints = len(X3d_list)

        sgn_sum = 0
        for a in range(nPoints):
            # Xak : 第kカメラ座標系から見た点aの座標。
            # p.217 eq.13.71
            Xa = X3d_list[a]

            Xa_tk = Xa - tk

            Xak = RkT @ Xa_tk
            sgn_sum += np.sign(Xak[2, 0])

        return sgn_sum

    def flip_X3d_sign(self, X3d_list):
        nPoints = len(X3d_list)

        for a in range(nPoints):
            Xa = X3d_list[a]
            X3d_list[a] = -Xa

        return X3d_list

    def Extract_Cam_Extrinsic(self, P_list, X_list, H, Kk_list):
        """
        p.216 §13.4 手順13.7
        各カメラの並進tと回転Rを求める。
        """
        nCams = len(P_list)
        nPoints = len(X_list)

        H_inv = np.linalg.inv(H)

        # p.216 eq.13.63
        X3d_list = self.build_X3d_list(X_list, H_inv)

        Rk_list = []
        tk_list = []

        for k in range(nCams):
            # p.216 eq.13.64, p.198 eq.13.3, p.199 カメラに関する知識を用いる方法の説明参照。
            Pk = P_list[k]
            Pk = Pk @ H

            # p.216 eq. 13.65
            Kk = Kk_list[k]
            KkInv_Pk = np.linalg.inv(Kk) @ Pk
            Ak = KkInv_Pk[:, 0:3]
            bk = np.vstack(KkInv_Pk[:, 3:4])

            # p.216 eq.13.66
            detAk = np.linalg.det(Ak)
            sign_detAk = np.sign(detAk)
            detAk = np.abs(detAk)
            s = detAk ** (1.0 / 3.0)
            if sign_detAk < 1.0:
                s = -s

            # p.217 eq.13.67
            Ak = Ak * (1.0 / s)
            bk = bk * (1.0 / s)

            # p.217 eq.13.68
            Ua, SigmaA, VaT = np.linalg.svd(Ak, full_matrices=False)

            # 元に戻る：OK
            # Ua_S_VaT = Ua @ np.diag(SigmaA) @ VaT

            # p.217 eq.13.69
            Rk = VaT.T @ Ua.T

            Rk_list.append(Rk)

            # p.217 eq.13.70
            tk = -Rk @ bk

            sgn_sum = self.calc_sum_all_points_Z_sgn(X3d_list, Rk.T, tk)
            if sgn_sum <= 0:
                # p.217 eq.13.72
                tk = -tk
                X3d_list = self.flip_X3d_sign(X3d_list)

            tk_list.append(tk)

        return Rk_list, tk_list, X3d_list


def Run_MultiCamSelfCalib(
    in_feature_point_list_path,
    result_campose_csv,
    result_campose_ply,
    result_points3d_ply,
    f0,
    reproj_err_threshold,
    J_threshold,
):
    fp_list = CSV_Read_FeaturePointList(in_feature_point_list_path, f0)

    nCams = fp_list[0].CameraCount()

    sc = MultiCamSelfCalib(f0)
    P_list, X_list = sc.PrimaryMethodFaster(fp_list, reproj_err_threshold)

    default_camFocalLen_list = [f0] * nCams

    # Kk : cam intrinsic mat
    H, Kk_list = sc.Euclidean_upgrade(default_camFocalLen_list, J_threshold)
    print(f"Kk_list=\n{Kk_list}")

    Rk_list, tk_list, X3d_list = sc.Extract_Cam_Extrinsic(P_list, X_list, H, Kk_list)

    # print(f"Rk_list={Rk_list}\ntk_list={tk_list}")

    CSV_Write_CamPose_list(result_campose_csv, tk_list, Rk_list)
    PLY_Export_MultiCam(result_campose_ply, tk_list, Rk_list)
    PLY_Export_PointList(result_points3d_ply, X3d_list)

    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="reads Run_FeatureMatch3 result csv, estimate cam pose"
    )

    parser.add_argument(
        "--in_feature_point_list_csv",
        type=str,
        help="input csv file",
        default="tmp/0000_0001_0002.csv",
    )

    parser.add_argument(
        "--f0",
        type=float,
        help="f0 param of csv file. Typically it is image size in pixel.",
        default=800,
    )
    parser.add_argument(
        "--reproj_err_converge",
        type=float,
        help="reprojection error converge iteration diff in pixel.",
        default=0.001,
    )
    parser.add_argument(
        "--j_threshold",
        type=float,
        help="Euclidean upgrade threshold in pixel.",
        default=1.0,
    )
    parser.add_argument(
        "--result_campose_csv",
        type=str,
        help="output camera pose CSV file",
        default="tmp/result_campose_0000_0001_0002.csv",
    )
    parser.add_argument(
        "--result_campose_ply",
        type=str,
        help="output camera pose PLY file",
        default="tmp/result_campose_0000_0001_0002.ply",
    )
    parser.add_argument(
        "--result_points3d_ply",
        type=str,
        help="output 3d points PLY file",
        default="tmp/result_points_0000_0001_0002.ply",
    )
    args = parser.parse_args()

    br = Run_MultiCamSelfCalib(
        args.in_feature_point_list_csv,
        args.result_campose_csv,
        args.result_campose_ply,
        args.result_points3d_ply,
        args.f0,
        args.reproj_err_converge,
        args.j_threshold,
    )
    if br == True:
        rv = 0
    else:
        rv = 1
    sys.exit(rv)
