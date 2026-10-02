import argparse
import csv
import itertools
import os
import statistics

import numpy as np

from Common import (
    CSV_Write_CamPose_list,
    CSV_Write_Point3d_list,
    Read_CamPose_CSV,
    inv_rigid,
    SelectF0_FromFeatureSpread,
)

from PLYUtils import PLY_Export_MultiCam, PLY_Export_PointNdArray

# r(カメラ座標z)のクランプ上限: r^4 オーバーフロー回避のため。
RMAX = 1e6


class BundleAdjustment:
    """
    金谷(『コンピュータビジョン』第11章)のLMバンドル調整のPython移植。

    カメラモデル:  P = K R^T [I | -t],  K = [[f,0,u],[0,f,v],[0,0,f0]]
    各カメラのパラメータ9個 = (f, u, v, tx, ty, tz, rx, ry, rz)。

    ゲージ正規化:  カメラ0を原点・単位回転とし、カメラ1の並進の
    絶対値最大成分を1に固定する(スケールの自由度を除く)。
    固定パラメータ: カメラ0のt,R(6個)と、カメラ1のゲージ軸成分t_a(1個)。

    観測座標は画像中心原点のピクセル座標(x+→, y+↓)をそのまま用いる
    (参照C++の width/height による軸交換変換はバイパス)。
    principal point (u,v) は自由パラメータとして最適化する(初期値 0)。
    """

    def __init__(
        self,
        f0,
        delta=0.0001,
        max_iter=400,
        max_inner=80,
        shared_intrinsic=False,
        pose_lambda=0.0,
        pose_w_reg=1.0,
        fixed_focal=None,
    ):
        self.f0 = float(f0)
        self.delta = delta
        self.max_iter = max_iter
        self.max_inner = max_inner
        self.shared_intrinsic = shared_intrinsic
        # fixed_focal: 既知の焦点距離 fx=fy (px)。None以外なら焦点距離を
        # 最適化せずこの値に固定する(f0の正規化とは独立)。
        self.fixed_focal = fixed_focal
        # 姿勢アンカー正則化: 目的関数に
        #   lambda * ( ||t_k - t_ref_k||^2 + pose_w_reg * ||log(R_k R_ref_k^T)||^2 )
        # を加え、BAを初期(merged)姿勢からの精緻化として動作させる。
        self.pose_lambda = float(pose_lambda)
        self.pose_w_reg = float(pose_w_reg)

    # ------------------------------------------------------------------
    def setParams(self, X_global, cam_t, cam_R, alpha_v, kappa_v, x_v, y_v, cam_ids):
        self.X_global0 = np.asarray(X_global, dtype=float)
        self.cam_t0 = np.asarray(cam_t, dtype=float)
        self.cam_R0 = np.asarray(cam_R, dtype=float)
        self.cam_ids = list(cam_ids)
        self.N = self.X_global0.shape[0]
        self.M = self.cam_t0.shape[0]
        self.V = alpha_v.shape[0]
        self.alpha_v = np.asarray(alpha_v, dtype=int)
        self.kappa_v = np.asarray(kappa_v, dtype=int)
        self.x_v = np.asarray(x_v, dtype=float)
        self.y_v = np.asarray(y_v, dtype=float)

        # ゲージ軸: カメラ0からカメラ1への並進の絶対値最大成分。
        R1 = self.cam_R0[0]
        t0 = self.cam_t0[0]
        d = R1.T @ (self.cam_t0[1] - t0)
        self.gauge_axis = int(np.argmax(np.abs(d)))
        self.s = float(d[self.gauge_axis])
        if abs(self.s) < 1e-12:
            raise ValueError(f"gauge scale too small: {self.s}")
        self.R1 = R1
        self.t0 = t0

        # ゲージ正規化: t' = 1/s R1^T(t-t0), R' = R1^T R, X' = 1/s R1^T(X-t0)。
        self.t = (1.0 / self.s) * np.einsum("ij,mj->mi", R1.T, self.cam_t0 - t0)
        self.R = np.einsum("ij,mjk->mik", R1.T, self.cam_R0)
        self.X = (1.0 / self.s) * np.einsum("ij,nj->ni", R1.T, self.X_global0 - t0)

        # 姿勢アンカー正則化の参照(ゲージ正規化座標での初期姿勢)。
        self.t_ref = self.t.copy()
        self.R_ref = self.R.copy()

        # カメラ内部パラメータ初期値(全カメラ共通、principal point=中心=0)。
        # f0 は座標正規化(K[2,2])、f は焦点距離(K[0,0]=K[1,1])で独立。
        if self.fixed_focal is not None:
            self.f = np.full(self.M, float(self.fixed_focal))
        else:
            self.f = np.full(self.M, self.f0)
        self.u = np.zeros(self.M)
        self.v = np.zeros(self.M)
        self.cameraMat = np.zeros((self.M, 3, 4))
        self.update_cameraMat()

        # 自由パラメータのコンパクト列マッピング。
        # 固定: カメラ0の t(3,4,5),R(6,7,8) と カメラ1の t_{gauge_axis}(=3+a)。
        # shared_intrinsic 時は、全カメラの f(param 0) が1つの共有列にマップされる
        # (同じ物理カメラで撮影した全画像の内部パラメータは同一という拘束)。
        self.colmap = np.full((self.M, 9), -1, dtype=int)
        a = self.gauge_axis
        idx = 0
        if self.shared_intrinsic and self.fixed_focal is None:
            self.colmap[:, 0] = idx  # 共有焦点距離 f の列
            idx += 1
        for kappa in range(self.M):
            for param in range(9):
                if (kappa == 0 and param in (3, 4, 5, 6, 7, 8)) or (
                    kappa == 1 and param == 3 + a
                ):
                    continue
                if self.fixed_focal is not None and param == 0:
                    continue  # 既知の焦点距離: 焦点距離を最適化しない(固定)
                if self.shared_intrinsic and param == 0:
                    continue  # 共有f列へは既に割り当て済み
                self.colmap[kappa, param] = idx
                idx += 1
        self.M9_7 = idx
        self.freecols = [
            self.colmap[k][self.colmap[k] >= 0].tolist() for k in range(self.M)
        ]
        self.free_mask = self.colmap >= 0
        self.denom = 2 * self.V - (3 * self.N + self.M9_7)
        self.c = 0.0001

        # 各点が見えるカメラ(疎可視性)。
        self.point_cams = [[] for _ in range(self.N)]
        for v in range(self.V):
            self.point_cams[self.alpha_v[v]].append(self.kappa_v[v])
        self.cam_points = [[] for _ in range(self.M)]
        for v in range(self.V):
            self.cam_points[self.kappa_v[v]].append(self.alpha_v[v])

        # 初期化時の再投影誤差を表示。
        self.calcpqr()
        self.calcError()
        print(
            f"BA init: N={self.N} points, M={self.M} cams, V={self.V} obs, "
            f"gauge axis={self.gauge_axis}, s={self.s:.6g}"
        )
        print(f"  initial error={self.error:.6g}")

    def update_cameraMat(self):
        K = np.zeros((self.M, 3, 3))
        K[:, 0, 0] = self.f
        K[:, 1, 1] = self.f
        K[:, 2, 2] = self.f0
        K[:, 0, 2] = self.u
        K[:, 1, 2] = self.v
        Rt = np.transpose(self.R, (0, 2, 1))
        It = np.zeros((self.M, 3, 4))
        It[:, :3, :3] = np.eye(3)
        It[:, :, 3] = -self.t
        self.cameraMat = np.einsum("mij,mjk->mik", K, np.einsum("mij,mjk->mik", Rt, It))

    def calcpqr(self):
        Xh = np.hstack([self.X, np.ones((self.N, 1))])
        cm = self.cameraMat[self.kappa_v]
        Xa = Xh[self.alpha_v]
        pqr = np.einsum("vij,vj->vi", cm, Xa)
        self.p = pqr[:, 0]
        self.q = pqr[:, 1]
        self.r = pqr[:, 2]

    def calcError(self):
        e = (
            (self.p / self.r - self.x_v / self.f0) ** 2
            + (self.q / self.r - self.y_v / self.f0) ** 2
        ).sum()
        self.error = e

    # ------------------------------------------------------------------
    # 各観測ペアの1次導関数。
    def _pair_derivs(self):
        p, q, r = self.p, self.q, self.r
        f0 = self.f0
        eql1 = p / r - self.x_v / f0
        eqr1 = q / r - self.y_v / f0

        # 3D点に関する導関数: dX = cameraMat[:, :, :3]。
        dXv = self.cameraMat[self.kappa_v][:, :, :3]

        # カメラパラメータに関する導関数 dC (V,3,9)。
        fk = self.f[self.kappa_v]
        uk = self.u[self.kappa_v]
        vk = self.v[self.kappa_v]
        Rk = self.R[self.kappa_v]
        r_k1 = Rk[:, :, 0]
        r_k2 = Rk[:, :, 1]
        r_k3 = Rk[:, :, 2]
        vec2 = self.X[self.alpha_v] - self.t[self.kappa_v]

        dCv = np.zeros((self.V, 3, 9))
        dCv[:, 0, 0] = (p - uk / f0 * r) / fk
        dCv[:, 1, 0] = (q - vk / f0 * r) / fk
        dCv[:, 0, 1] = r / f0
        dCv[:, 1, 2] = r / f0
        for c in range(3):
            dCv[:, 0, 3 + c] = -(fk * r_k1[:, c] + uk * r_k3[:, c])
            dCv[:, 1, 3 + c] = -(fk * r_k2[:, c] + vk * r_k3[:, c])
            dCv[:, 2, 3 + c] = -f0 * r_k3[:, c]
        dCv[:, :, 6] = np.cross(fk[:, None] * r_k1 + uk[:, None] * r_k3, vec2)
        dCv[:, :, 7] = np.cross(fk[:, None] * r_k2 + vk[:, None] * r_k3, vec2)
        dCv[:, :, 8] = np.cross(f0 * r_k3, vec2)

        # eql/eqr 型の量(外積項)。カメラから極端に遠い点(rが巨大)は
        # 微分が0に近づくため、オーバーフローを避けるため r をクランプする。
        rc = np.clip(r, -RMAX, RMAX)
        Lp = rc[:, None] * dXv[:, 0, :] - p[:, None] * dXv[:, 2, :]  # (V,3) 点座標
        Rp = rc[:, None] * dXv[:, 1, :] - q[:, None] * dXv[:, 2, :]
        Lc = rc[:, None] * dCv[:, 0, :] - p[:, None] * dCv[:, 2, :]  # (V,9) カメラ
        Rc = rc[:, None] * dCv[:, 1, :] - q[:, None] * dCv[:, 2, :]
        self._rc = rc
        return eql1, eqr1, dXv, dCv, Lp, Rp, Lc, Rc

    def _r4(self):
        return self._rc**4

    def calcdError(self):
        eql1, eqr1, dXv, dCv, Lp, Rp, Lc, Rc = self._pair_derivs()
        r2 = self._rc * self._rc
        derror_point = np.zeros((self.N, 3))
        for i in range(3):
            term = 2 * (eql1 * Lp[:, i] + eqr1 * Rp[:, i]) / r2
            np.add.at(derror_point[:, i], self.alpha_v, term)
        self.derror_point = derror_point
        derror_cam = np.zeros((self.M, 9))
        for i in range(9):
            term = 2 * (eql1 * Lc[:, i] + eqr1 * Rc[:, i]) / r2
            np.add.at(derror_cam[:, i], self.kappa_v, term)
        self.derror_cam = derror_cam
        self._add_pose_prior(derror_cam)

    def _calchE(self, pd):
        eql1, eqr1, dXv, dCv, Lp, Rp, Lc, Rc = pd
        r4 = self._r4()
        contrib = (
            2
            * (Lp[:, :, None] * Lp[:, None, :] + Rp[:, :, None] * Rp[:, None, :])
            / r4[:, None, None]
        )
        hE = np.zeros((self.N, 3, 3))
        np.add.at(hE, self.alpha_v, contrib)
        c = self.c
        hE[:, 0, 0] *= 1.0 + c
        hE[:, 1, 1] *= 1.0 + c
        hE[:, 2, 2] *= 1.0 + c
        self.hE = hE

    def _calchF(self, pd):
        eql1, eqr1, dXv, dCv, Lp, Rp, Lc, Rc = pd
        r4 = self._r4()
        M9_7 = self.M9_7
        hF = np.zeros((self.N, 3, M9_7))
        hFf = hF.reshape(-1)
        kappa = self.kappa_v
        alpha = self.alpha_v
        for i in range(9):
            col = self.colmap[:, i][kappa]
            valid = col >= 0
            av = alpha[valid]
            cv = col[valid]
            for j in range(3):
                term = 2 * (Lc[:, i] * Lp[:, j] + Rc[:, i] * Rp[:, j]) / r4
                idx = (av * 3 + j) * M9_7 + cv
                np.add.at(hFf, idx, term[valid])
        self.hF = hF

    def _calchG(self, pd):
        eql1, eqr1, dXv, dCv, Lp, Rp, Lc, Rc = pd
        r4 = self._r4()
        G = (
            2
            * (Lc[:, :, None] * Lc[:, None, :] + Rc[:, :, None] * Rc[:, None, :])
            / r4[:, None, None]
        )
        hG = np.zeros((self.M9_7, self.M9_7))
        kappa = self.kappa_v
        for i in range(9):
            ri = self.colmap[:, i]
            for j in range(9):
                rj = self.colmap[:, j]
                ci = ri[kappa]
                cj = rj[kappa]
                valid = (ci >= 0) & (cj >= 0)
                np.add.at(hG, (ci[valid], cj[valid]), G[valid, i, j])
        hG[np.arange(self.M9_7), np.arange(self.M9_7)] *= 1.0 + self.c
        self._add_pose_prior(np.zeros((self.M, 9)), hG=hG)
        self.hG = hG

    def calcddError(self):
        pd = self._pair_derivs()
        self._calchE(pd)
        self._calchF(pd)
        self._calchG(pd)

    # ------------------------------------------------------------------
    def getdeltaXiF(self, kappa, param):
        col = self.colmap[kappa, param]
        return self.deltaXiF[col] if col >= 0 else 0.0

    def getRotateMat(self, w):
        theta = float(np.linalg.norm(w))
        if theta < 1e-10:
            return np.eye(3)
        a = w / theta
        K = np.array(
            [
                [0.0, -a[2], a[1]],
                [a[2], 0.0, -a[0]],
                [-a[1], a[0], 0.0],
            ]
        )
        return np.eye(3) + np.sin(theta) * K + (1.0 - np.cos(theta)) * (K @ K)

    def _rotvec(self, R):
        """回転行列Rのlog map(回転ベクトル)。getRotateMatの逆。"""
        tr = np.clip((np.trace(R) - 1.0) / 2.0, -1.0, 1.0)
        theta = np.arccos(tr)
        v = np.array(
            [
                R[2, 1] - R[1, 2],
                R[0, 2] - R[2, 0],
                R[1, 0] - R[0, 1],
            ]
        )
        s = 0.5 * np.sin(theta)
        if abs(s) < 1e-12:
            return 0.5 * v
        return v * (theta / (2.0 * s))

    def _add_pose_prior(self, derror_cam, hG=None):
        """姿勢アンカー正則化の1次/2次微分を加える。"""
        lam = self.pose_lambda
        if lam <= 0:
            return
        w = self.pose_w_reg
        for k in range(self.M):
            derror_cam[k, 3:6] += 2.0 * lam * (self.t[k] - self.t_ref[k])
            om = self._rotvec(self.R[k] @ self.R_ref[k].T)
            derror_cam[k, 6:9] += 2.0 * lam * w * om
            if hG is not None:
                for param in (3, 4, 5):
                    col = self.colmap[k, param]
                    if col >= 0:
                        hG[col, col] += 2.0 * lam
                for param in (6, 7, 8):
                    col = self.colmap[k, param]
                    if col >= 0:
                        hG[col, col] += 2.0 * lam * w

    def solveEquations(self):
        derror_point = self.derror_point
        derror_cam = self.derror_cam
        # 共有列(共有f等)へは複数カメラの微分が合算される。
        self.dF = np.zeros(self.M9_7)
        for kappa in range(self.M):
            for param in range(9):
                col = self.colmap[kappa, param]
                if col >= 0:
                    self.dF[col] += derror_cam[kappa, param]

        invhE = np.linalg.inv(self.hE)
        M9_7 = self.M9_7
        traFaFa = np.zeros((M9_7, M9_7))
        traFaE = np.zeros(M9_7)
        for alpha in range(self.N):
            Einv = invhE[alpha]
            nab = derror_point[alpha]
            hFa = self.hF[alpha]
            for kappa in self.point_cams[alpha]:
                cols = self.freecols[kappa]
                Fblk = hFa[:, cols]
                FE = Fblk.T @ Einv
                traFaFa[np.ix_(cols, cols)] += FE @ Fblk
                traFaE[cols] += FE @ nab

        matl = self.hG - traFaFa
        matr = traFaE - self.dF
        try:
            self.deltaXiF = np.linalg.solve(matl, matr)
        except np.linalg.LinAlgError:
            self.deltaXiF = np.linalg.lstsq(matl, matr, rcond=None)[0]

        FdXiF = np.einsum("nij,j->ni", self.hF, self.deltaXiF)
        self.deltaXiP = -np.einsum("nij,nj->ni", invhE, FdXiF + derror_point)

    def calcErrorTilde(self):
        XT = self.X + self.deltaXiP
        XTh = np.hstack([XT, np.ones((self.N, 1))])
        fT = self.f.copy()
        tT = self.t.copy()
        RT = self.R.copy()
        uT = self.u.copy()
        vT = self.v.copy()
        for kappa in range(self.M):
            fT[kappa] += self.getdeltaXiF(kappa, 0)
            uT[kappa] += self.getdeltaXiF(kappa, 1)
            vT[kappa] += self.getdeltaXiF(kappa, 2)
            dlt = np.array(
                [
                    self.getdeltaXiF(kappa, 3),
                    self.getdeltaXiF(kappa, 4),
                    self.getdeltaXiF(kappa, 5),
                ]
            )
            tT[kappa] = self.t[kappa] + dlt
            om = np.array(
                [
                    self.getdeltaXiF(kappa, 6),
                    self.getdeltaXiF(kappa, 7),
                    self.getdeltaXiF(kappa, 8),
                ]
            )
            RT[kappa] = self.getRotateMat(om) @ self.R[kappa]

        K = np.zeros((self.M, 3, 3))
        K[:, 0, 0] = fT
        K[:, 1, 1] = fT
        K[:, 2, 2] = self.f0
        K[:, 0, 2] = uT
        K[:, 1, 2] = vT
        Rtt = np.transpose(RT, (0, 2, 1))
        It = np.zeros((self.M, 3, 4))
        It[:, :3, :3] = np.eye(3)
        It[:, :, 3] = -tT
        cmT = np.einsum("mij,mjk->mik", K, np.einsum("mij,mjk->mik", Rtt, It))
        pqr = np.einsum("vij,vj->vi", cmT[self.kappa_v], XTh[self.alpha_v])
        pT = pqr[:, 0]
        qT = pqr[:, 1]
        rT = pqr[:, 2]
        self.errorTilde = (
            (pT / rT - self.x_v / self.f0) ** 2 + (qT / rT - self.y_v / self.f0) ** 2
        ).sum()

    def renewParams(self):
        self.X = self.X + self.deltaXiP
        for kappa in range(self.M):
            self.f[kappa] += self.getdeltaXiF(kappa, 0)
            self.u[kappa] += self.getdeltaXiF(kappa, 1)
            self.v[kappa] += self.getdeltaXiF(kappa, 2)
            dlt = np.array(
                [
                    self.getdeltaXiF(kappa, 3),
                    self.getdeltaXiF(kappa, 4),
                    self.getdeltaXiF(kappa, 5),
                ]
            )
            self.t[kappa] = self.t[kappa] + dlt
            om = np.array(
                [
                    self.getdeltaXiF(kappa, 6),
                    self.getdeltaXiF(kappa, 7),
                    self.getdeltaXiF(kappa, 8),
                ]
            )
            self.R[kappa] = self.getRotateMat(om) @ self.R[kappa]
        self.update_cameraMat()

    def bundleAdjustment(self, verbose=True):
        self.calcpqr()
        self.calcError()
        e = self.f0 * np.sqrt(self.error / self.denom)
        self.c = 0.0001
        loop = 0
        while loop < self.max_iter:
            self.calcdError()
            self.calcddError()
            inner = 0
            step_ok = False
            while True:
                self.solveEquations()
                self.calcErrorTilde()
                eTilde = self.f0 * np.sqrt(self.errorTilde / self.denom)
                if verbose:
                    print(f"    c={self.c:.3e}  e={e:.6e}  ~e={eTilde:.6e}")
                if eTilde > e:
                    self.c = 10.0 * self.c
                    self.calcdError()
                    self.calcddError()
                    inner += 1
                    if inner > self.max_inner:
                        print("  BA: inner loop cap reached")
                        break
                else:
                    step_ok = True
                    break
            if not step_ok:
                # 誤差を減らすステップが見つからなかった。
                # 誤差増加のステップを適用せず終了する。
                print("  BA: no decreasing step found; stop")
                eTilde = e
                break
            self.renewParams()
            self.calcpqr()
            self.calcError()
            loop += 1
            new_e = self.f0 * np.sqrt(self.error / self.denom)
            if verbose:
                print(f"  loop {loop}: e~={eTilde:.6e}  actual={new_e:.6e}")
            if abs(eTilde - e) <= self.delta:
                break
            e = eTilde
            self.c = self.c / 10.0
        return loop

    def get_merged(self):
        """ゲージ正規化座標からマージフレーム座標へ戻す。"""
        R1 = self.R1
        t0 = self.t0
        s = self.s
        t_out = t0 + s * np.einsum("ij,mj->mi", R1, self.t)
        R_out = np.einsum("ij,mjk->mik", R1, self.R)
        X_out = t0 + s * np.einsum("ij,nj->ni", R1, self.X)
        return t_out, R_out, X_out, self.f.copy()


def _derive_sibling(path, tag):
    """camPose_0000_0001_0002.csv -> {tag}_0000_0001_0002.csv"""
    d, name = os.path.split(path)
    stem = name
    for pfx in ("camPose", "featurePoints2d", "points3d"):
        if stem.startswith(pfx):
            stem = stem[len(pfx) :]
            break
    return os.path.join(d, f"{tag}{stem}")


def _read_feature_points(path):
    cam_tri = []
    obs = []
    with open(path, newline="") as f:
        r = csv.reader(f)
        next(r)
        for row in r:
            if len(row) < 9:
                continue
            try:
                cids = [int(row[0]), int(row[3]), int(row[6])]
                xy = [
                    float(row[1]),
                    float(row[2]),
                    float(row[4]),
                    float(row[5]),
                    float(row[7]),
                    float(row[8]),
                ]
            except (ValueError, IndexError):
                continue
            cam_tri.append(cids)
            obs.append(xy)
    return cam_tri, np.array(obs)


def _read_points3d(path):
    pts = []
    with open(path, newline="") as f:
        r = csv.reader(f)
        next(r)
        for row in r:
            if len(row) < 3:
                continue
            try:
                pts.append([float(row[0]), float(row[1]), float(row[2])])
            except (ValueError, IndexError):
                continue
    return np.array(pts)


def Build_Global_BA_Data(cam_pose_csv_list, merged_pose):
    """
    各トリプレットの points3d(ローカルフレーム)と featurePoints2d を読み、
    マージフレームのグローバル点列と観測セットに変換する。

    merged_pose: {cam_id: (R, t)} (マージフレーム、cam0=原点)。
    returns (X_global(N,3), cam_tri_all(N,3), obs_all(N,6))。
    """
    X_list = []
    cam_tri_all = []
    obs_all = []
    for path in cam_pose_csv_list:
        feat = _derive_sibling(path, "featurePoints2d")
        pts3 = _derive_sibling(path, "points3d")
        local = {c: M for c, M, _jk, _cf in Read_CamPose_CSV(path)}
        a = min(local.keys())
        Rl_a, tl_a = local[a][:3, :3], local[a][:3, 3]
        Rm_a, tm_a = merged_pose[a]

        # ローカルフレーム→マージフレームは相似変換(スケールsを含む)。
        # 各トリプレットは任意の絶対スケールを持つため、剛体変換では点群の
        # スケールがマージカメラと一致しない(点群だけがずれる原因)。
        R = Rm_a @ Rl_a.T
        # スケール: トリプレット内カメラペアのベースライン長の比(マージ/ローカル)の中央値。
        s_list = []
        cams_local = sorted(local.keys())
        for ca, cb in itertools.combinations(cams_local, 2):
            d_loc = np.linalg.norm(local[ca][:3, 3] - local[cb][:3, 3])
            d_mer = np.linalg.norm(merged_pose[ca][1] - merged_pose[cb][1])
            if d_loc > 1e-12:
                s_list.append(d_mer / d_loc)
        s = statistics.median(s_list) if s_list else 1.0
        t = tm_a - s * (R @ tl_a)

        P_local = _read_points3d(pts3)
        cam_tri, obs = _read_feature_points(feat)

        n = len(obs)
        if len(P_local) != n:
            print(f"  WARN: {os.path.basename(path)}: points={len(P_local)} obs={n}")
        # 完全一致の重複行を除去(先頭のインデックスを保持し、points3dと整列)。
        seen = set()
        keep = []
        for idx in range(n):
            key = tuple(obs[idx])
            if key not in seen:
                seen.add(key)
                keep.append(idx)
        P_local = P_local[keep]
        obs = obs[keep]
        cam_tri = [cam_tri[i] for i in keep]

        Pg = s * (R @ P_local.T).T + t
        for i in range(len(Pg)):
            X_list.append(Pg[i])
            cam_tri_all.append(cam_tri[i])
            obs_all.append(obs[i])
    return np.array(X_list), cam_tri_all, np.array(obs_all)


def Run_BundleAdjustment(
    in_cam_pose_csv_list,
    merged_cam_pose_csv,
    out_cam_pose_csv,
    out_cam_pose_ply=None,
    out_points3d_csv=None,
    out_points3d_ply=None,
    f0=None,
    delta=0.0001,
    max_iter=200,
    shared_intrinsic=False,
    pose_lambda=0.0,
    pose_w_reg=1.0,
    fixed_focal=None,
    verbose=True,
):
    """グローバルリングバンドル調整を実行する。"""
    merged = Read_CamPose_CSV(merged_cam_pose_csv)
    cam_ids = sorted(c[0] for c in merged)
    idx_of = {cid: i for i, cid in enumerate(cam_ids)}
    M = len(cam_ids)
    cam_t = np.zeros((M, 3))
    cam_R = np.zeros((M, 3, 3))
    for cid, Mp, _jk, _cf in merged:
        i = idx_of[cid]
        cam_t[i] = Mp[:3, 3]
        cam_R[i] = Mp[:3, :3]
    merged_pose = {cid: (Mp[:3, :3], Mp[:3, 3]) for cid, Mp, _jk, _cf in merged}

    X_global, cam_tri_all, obs_all = Build_Global_BA_Data(
        in_cam_pose_csv_list, merged_pose
    )

    alpha_v = []
    kappa_v = []
    x_v = []
    y_v = []
    for alpha, (cids, xy) in enumerate(zip(cam_tri_all, obs_all)):
        for k in range(3):
            alpha_v.append(alpha)
            kappa_v.append(idx_of[cids[k]])
            x_v.append(xy[2 * k])
            y_v.append(xy[2 * k + 1])
    alpha_v = np.array(alpha_v)
    kappa_v = np.array(kappa_v)
    x_v = np.array(x_v)
    y_v = np.array(y_v)

    if f0 is None:
        f0 = SelectF0_FromFeatureSpread(
            _derive_sibling(in_cam_pose_csv_list[0], "featurePoints2d")
        )
    print(f"f0 = {f0:.4f}")

    ba = BundleAdjustment(
        f0,
        delta=delta,
        max_iter=max_iter,
        shared_intrinsic=shared_intrinsic,
        pose_lambda=pose_lambda,
        pose_w_reg=pose_w_reg,
        fixed_focal=fixed_focal,
    )
    ba.setParams(X_global, cam_t, cam_R, alpha_v, kappa_v, x_v, y_v, cam_ids)
    loop = ba.bundleAdjustment(verbose=verbose)

    t_out, R_out, X_out, f_out = ba.get_merged()
    CSV_Write_CamPose_list(
        out_cam_pose_csv,
        [t.reshape(3, 1) for t in t_out],
        [R for R in R_out],
        cam_ids,
    )
    if out_cam_pose_ply:
        PLY_Export_MultiCam(
            out_cam_pose_ply, [t.reshape(3, 1) for t in t_out], [R for R in R_out]
        )
    if out_points3d_csv:
        CSV_Write_Point3d_list(out_points3d_csv, [p.reshape(3, 1) for p in X_out])
    if out_points3d_ply:
        PLY_Export_PointNdArray(out_points3d_ply, np.asarray(X_out))

    # error は正規化座標 (p/r - x/f0)^2 の和。px 単位の RMSE には f0 を掛ける。
    rmse_px = ba.f0 * np.sqrt(ba.error / ba.V)
    print(
        f"BA: {loop} iterations, final RMSE = {rmse_px:.4f} px, "
        f"f0*std = {ba.f0 * np.sqrt(ba.error / ba.denom):.6e}"
    )
    print(
        f"focal lengths: min={f_out.min():.2f} max={f_out.max():.2f} "
        f"mean={f_out.mean():.2f}"
    )
    return True
