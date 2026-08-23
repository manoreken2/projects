import argparse
import csv
from Common import *
import numpy as np
import cv2 as cv
import os
import sys
import statistics
from PLYUtils import PLY_Export_MultiCam, PLY_Export_PointList


def SelectF0_FromFeatureSpread(feature_point_list_csv):
    """
    3カメラ対応点CSVの特徴点座標の広がりから、f0を自動決定する。

    f0は、画像中心を原点とした特徴点座標(u, v)が、
    正規化ベクトル x_ak = [u/f0, v/f0, 1] においてO(1)になるよう、
    座標の代表的な広がり(最大絶対値)に合わせる。

    戻り値: 推定したf0。CSVが読めない/座標が無い場合はNone。
    """
    abs_u = []
    abs_v = []
    with open(feature_point_list_csv) as f:
        r = csv.reader(f, delimiter=",")
        for l in r:
            # ヘッダ行や空行をスキップ。
            if len(l) < 9:
                continue
            try:
                # cam_id_A, xA, yA, cam_id_B, xB, yB, cam_id_C, xC, yC
                for i in range(3):
                    abs_u.append(abs(float(l[1 + 3 * i])))
                    abs_v.append(abs(float(l[2 + 3 * i])))
            except (ValueError, TypeError):
                continue

    if not abs_u:
        return None

    # 外れ値の影響を抑えるため、99.5パーセンタイルで代表させる。
    u_q = np.percentile(abs_u, 99.5)
    v_q = np.percentile(abs_v, 99.5)
    f0 = max(u_q, v_q)

    if f0 <= 0:
        return None
    return f0


class MultiCamSelfCalib:
    def __init__(self, f0, shared_intrinsic=False):
        self.f0 = f0
        # shared_intrinsic: 同じ物理カメラで撮影した全画像の内部パラメータは
        # 同一という拘束を課し、ユークリッドアップグレードのKkを全カメラ共通化する。
        self.shared_intrinsic = shared_intrinsic

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

    def DualMethodFaster(self, fp_list, reproj_err_converge_diff):
        """
        双対法 (faster版)。
        dual_method.cc faster_dual_method 相当。

        射影深度z_akの更新を、カメラごとに
        C行列(全特徴点分)のSVDで行うことで高速化する。

        reproj_err_converge_diff: 再投影誤差改善量の打ち切り値(pixel)

        """
        nPoints = len(fp_list)
        nCams = fp_list[0].CameraCount()

        z_ak_mat = np.ones((nPoints, nCams))

        # W: 観測行列 3MxN
        # (初回は、ループの外で3行ブロック正規化する。)
        W = build_observe_mat_W(fp_list, z_ak_mat)
        W = normalize_each_3rows(W)

        prev_reproj_err = sys.float_info.max
        for loop in range(10000):
            # WのSVDから、Vの最初の4列 v (Nx4) を取り出す。
            U_, Sigma, Vh_ = np.linalg.svd(W, full_matrices=False)
            v = Vh_.T[:, 0:4]

            # 各カメラについて射影的奥行きを更新する。
            for kp in range(nCams):
                # C = [C1 | C2 | C3]: N x 12 行列
                C = np.zeros((nPoints, 12))
                for al in range(nPoints):
                    xa = fp_list[al].x_ak(kp)
                    xa_nrm = np.linalg.norm(xa)
                    val = v[al, :] / xa_nrm
                    C[al, 0:4] = xa[0, 0] * val
                    C[al, 4:8] = xa[1, 0] * val
                    C[al, 8:12] = xa[2, 0] * val

                # CのSVD。特異値は降順なので、
                # 左特異ベクトルの第0列が最大特異値に対応。
                Uc, _, _ = np.linalg.svd(C, full_matrices=False)
                xi = Uc[:, 0]

                # xiの符号を選ぶ。
                if np.sum(xi) < 0:
                    xi = -xi

                # 射影的奥行き更新。
                for al in range(nPoints):
                    z = xi[al] / np.linalg.norm(fp_list[al].x_ak(kp))
                    z_ak_mat[al, kp] = z

            # Wを再構築する。
            W = build_observe_mat_W(fp_list, z_ak_mat)
            W = normalize_each_3rows(W)

            # X: v^T の各列 (= v の各行の転置, 4x1)
            X_list = []
            for al in range(nPoints):
                X = v[al, :]
                X = X.reshape(4, 1)
                X_list.append(X)

            # P: Wの各カメラブロック (3xN) と v の積 (3x4)
            P_list = []
            for kp in range(nCams):
                P = W[3 * kp : 3 * kp + 3, :] @ v
                P_list.append(P)

            reproj_err = self.calc_reproj_err(fp_list, P_list, X_list)
            print(
                f"DualMethodFaster {loop} reproj_err={reproj_err}, thr={reproj_err_converge_diff}"
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

    def _improve_shared_Kk(self, Omega, Kk_list, Qk_list):
        """
        全カメラで共通のKk(単一の焦点距離f)を推定する。

        各カメラの絶対二次曲線拘束から個別の焦点距離候補fkを求め、
        それらをロバストに統合(中央値)した単一fを全カメラに適用する。
        これにより、カメラごとのKkが個別に発散するのを防ぐ。
        """
        nCams = len(Kk_list)
        f0 = self.f0

        f_new = []
        Jk_list = []
        for k in range(nCams):
            Kk = Kk_list[k]
            Qk = Qk_list[k]
            QkOQkT = Qk @ Omega @ Qk.T
            ck11 = QkOQkT[0, 0]
            ck22 = QkOQkT[1, 1]
            ck33 = QkOQkT[2, 2]
            ck12 = QkOQkT[0, 1]
            ck13 = QkOQkT[0, 2]
            ck23 = QkOQkT[1, 2]
            ck31 = QkOQkT[2, 0]

            # p.213 eq.13.53
            Fk = ((ck11 + ck22) / ck33) - ((ck13 / ck33) ** 2) - ((ck23 / ck33) ** 2)
            if ck33 <= 0 or Fk <= 0:
                f_new.append(np.nan)
                Jk_list.append(sys.float_info.max)
                continue

            # p.213 eq.13.54 (光軸点の修正はスキップ du0k=dv0k=0)
            dfk = np.sqrt(0.5 * (((ck11 + ck22) / ck33) - 0.0 - 0.0))
            f_new.append(Kk[0, 0] * dfk)

            # p.215 eq.13.60
            Jk_list.append(
                ((ck11 / ck33 - 1.0) ** 2)
                + ((ck22 / ck33 - 1.0) ** 2)
                + 2.0 * (ck12**2 + ck23**2 + ck31**2) / (ck33**2)
            )

        valid = [f for f in f_new if np.isfinite(f)]
        if valid:
            f_shared = float(np.median(valid))
        else:
            f_shared = Kk_list[0][0, 0]

        shared_Kk = np.array([[f_shared, 0, 0], [0, f_shared, 0], [0, 0, f0]])
        return [shared_Kk.copy() for _ in range(nCams)], Jk_list

    def Euclidean_upgrade(self, camFocalLen_list, J_threshold, shared_intrinsic=False):
        """
        p.215 手順13.6
        """
        Jmed = sys.float_info.max
        Kk_list = self.build_initial_Kk_list(camFocalLen_list)

        prev_Jmed = Jmed

        # 最良(最小Jmed)の解を追跡する。
        # Jkが振動してオーバーシュートした場合、悪い解を返すのを防ぐ。
        best_Jmed = sys.float_info.max
        best_H = None
        best_Kk_list = None
        best_Jk_list = None

        while True:
            # Jk_list は calc_Omega で用いた Kk_list(improve前)に対応するため、
            # 最良解は improve 前の Kk_list と H のペアで保持する。
            Kk_list_pre = Kk_list
            Omega, H, Qk_list = self.calc_Omega(Kk_list_pre)

            if shared_intrinsic:
                Kk_list, Jk_list = self._improve_shared_Kk(Omega, Kk_list_pre, Qk_list)
            else:
                Kk_list, Jk_list = self.improve_Kk_list(Omega, Kk_list_pre, Qk_list)

            # p.215 eq.13.61
            Jmed = statistics.median(Jk_list)
            print(f"Euclidean_upgrade Jmed={Jmed}")
            if Jmed < best_Jmed:
                best_Jmed = Jmed
                best_H = H
                best_Kk_list = Kk_list_pre
                best_Jk_list = Jk_list

            if Jmed < J_threshold or Jmed >= prev_Jmed:
                # 終了条件達成。
                converged = best_Jmed < J_threshold
                return best_H, best_Kk_list, best_Jk_list, converged

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
    result_points3d_csv,
    result_points3d_ply,
    reproj_err_threshold,
    J_threshold,
    f0=-1,
    cam_id_list=None,
    method="dual",
    shared_intrinsic=False,
):
    if f0 < 0:
        f0_auto = SelectF0_FromFeatureSpread(in_feature_point_list_path)
        if f0_auto is None:
            raise RuntimeError("auto f0: failed to compute!")

        print(f"auto f0 = {f0_auto}")
        f0 = f0_auto

    fp_list = CSV_Read_FeaturePointList(in_feature_point_list_path, f0)

    nCams = fp_list[0].CameraCount()

    sc = MultiCamSelfCalib(f0, shared_intrinsic=shared_intrinsic)
    if method == "primary":
        P_list, X_list = sc.PrimaryMethodFaster(fp_list, reproj_err_threshold)
    elif method == "dual":
        P_list, X_list = sc.DualMethodFaster(fp_list, reproj_err_threshold)
    else:
        raise ValueError(f"unknown method: {method}")

    default_camFocalLen_list = [f0] * nCams

    # Kk : cam intrinsic mat
    H, Kk_list, Jk_list, converged = sc.Euclidean_upgrade(
        default_camFocalLen_list, J_threshold, shared_intrinsic
    )
    print(f"Kk_list=\n{Kk_list}")
    print(f"per-camera upgrade cost Jk={np.round(np.asarray(Jk_list, dtype=float), 4)}")

    if not converged:
        marker = result_campose_csv + ".failed"
        with open(marker, "w") as f:
            f.write("Euclidean upgrade did not converge (Jmed >= threshold)\n")
        print(
            f"WARNING: Euclidean upgrade did not converge "
            f"(Jmed >= {J_threshold}). Wrote marker {marker}"
        )

    Rk_list, tk_list, X3d_list = sc.Extract_Cam_Extrinsic(P_list, X_list, H, Kk_list)

    # print(f"Rk_list={Rk_list}\ntk_list={tk_list}")

    # カメラごとのユークリッド復元コスト Jk から confidence(0,1]を求める。
    # Jk: 大きい(=絶対二次曲線の拘束が破れている)カメラほど低い信頼度。
    # confidence :大きいほど信頼度が高い。
    confidence_list = 1.0 / (1.0 + np.asarray(Jk_list, dtype=float))

    # カメラのポーズを、カメラ0が単位上列になるよう変換。
    cam0inv = np.linalg.inv(Trans_Rot_to_CameraPoseMat(tk_list[0], Rk_list[0]))
    tk_list, Rk_list = CamTkRkTransform(tk_list, Rk_list, cam0inv)
    X3d_list = Point3dListTransform(X3d_list, cam0inv)

    CSV_Write_CamPose_list(
        result_campose_csv, tk_list, Rk_list, cam_id_list, Jk_list, confidence_list
    )
    CSV_Write_Point3d_list(result_points3d_csv, X3d_list)
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
        default="tmp/featurePoints2d_0000_0001_0002.csv",
    )

    parser.add_argument(
        "--cam1_id",
        type=int,
        default=1,
        help="camera id 1 (written to camera pose CSV).",
    )
    parser.add_argument(
        "--cam2_id",
        type=int,
        default=2,
        help="camera id 2 (written to camera pose CSV).",
    )
    parser.add_argument(
        "--cam3_id",
        type=int,
        default=3,
        help="camera id 3 (written to camera pose CSV).",
    )
    parser.add_argument(
        "--no_auto_f0",
        action="store_false",
        dest="auto_f0",
        default=True,
        help="disable auto f0 selection (use --f0 value).",
    )
    parser.add_argument(
        "--f0",
        type=float,
        help="f0 param in pixel. if negative value is specified, f0 is calculated from feature point spread.",
        default=-1,
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
        "--method",
        type=str,
        choices=["primary", "dual"],
        help="perspective self calibration method.",
        default="dual",
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
        "--result_points3d_csv",
        type=str,
        help="output 3d points CSV file",
        default="tmp/result_points_0000_0001_0002.csv",
    )
    parser.add_argument(
        "--result_points3d_ply",
        type=str,
        help="output 3d points PLY file",
        default="tmp/result_points_0000_0001_0002.ply",
    )
    parser.add_argument(
        "--shared_intrinsic",
        action="store_true",
        help="constrain all cameras to share one common intrinsic K "
        "(same physical camera assumption)",
    )
    args = parser.parse_args()

    br = Run_MultiCamSelfCalib(
        args.in_feature_point_list_csv,
        args.result_campose_csv,
        args.result_campose_ply,
        args.result_points3d_csv,
        args.result_points3d_ply,
        args.reproj_err_converge,
        args.j_threshold,
        args.f0,
        [args.cam1_id, args.cam2_id, args.cam3_id],
        args.method,
        args.shared_intrinsic,
    )
    if br == True:
        rv = 0
    else:
        rv = 1
    sys.exit(rv)
