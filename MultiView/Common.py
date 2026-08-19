from threading import ThreadError
import matplotlib.pyplot as plt
import csv
import numpy as np
from numpy.linalg import eigh
import math
import scipy.linalg as scipy_linalg
import os

# 右手座標系。
# 行列の要素の並びは行優先。
# トランスフォーム行列Mとベクトルvの積は
# M * v ：縦ベクトルvを右から掛ける。


# 2次元点群のペア。a: xy0_list と b: xy1_listを収容する。
class Point2dPair:
    def __init__(self, a, b):
        N = a.shape[0]
        assert N == b.shape[0]
        self.N = N

        self.a = a
        self.b = b

    def get_point_count(self):
        return self.N


# 3次元座標の回転行列作成。
def RotX(x):
    c = math.cos(x)
    s = math.sin(x)

    # 行優先: 要素は以下の表現の通りに並ぶ。
    m = np.array([[1, 0, 0, 0], [0, c, -s, 0], [0, s, c, 0], [0, 0, 0, 1]])
    return m


def RotY(y):
    c = math.cos(y)
    s = math.sin(y)
    m = np.array([[c, 0, s, 0], [0, 1, 0, 0], [-s, 0, c, 0], [0, 0, 0, 1]])
    return m


def RotZ(z):
    c = math.cos(z)
    s = math.sin(z)
    m = np.array([[c, -s, 0, 0], [s, c, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]])
    return m


# rx ry rzの順に回転。
def CamPoseXYZ(rx, ry, rz, pxyz):
    mX = RotX(rx)
    mY = RotY(ry)
    mZ = RotZ(rz)
    m = mZ @ mY @ mX
    m[0:3, 3] = pxyz[0:3]
    print(f"CamPoseXYZ=\n{m}")
    return m


# 縦ベクトル
def Proj(fovX, fovY, zNear, zFar):
    m = np.eye(4)
    w = 1.0 / math.tan(fovX / 2)
    h = 1.0 / math.tan(fovY / 2)
    q = zFar / (zFar - zNear)
    m[0, 0] = w
    m[1, 1] = h
    m[2, 2] = q
    m[3, 2] = 1
    m[2, 3] = -q * zNear
    m[3, 3] = 0
    return m


# Z-向きのカメラのメッシュ vertex listとtriangle list。
def Generate_CameraMeshZM(scale=0.1):
    sz = scale * 1.5
    sx = scale
    sy = scale * 9.0 / 16.0
    p = np.zeros((5, 3))
    p[0] = np.array((0, 0, 0))
    p[1] = np.array((sx, sy, -sz))
    p[2] = np.array((sx, -sy, -sz))
    p[3] = np.array((-sx, -sy, -sz))
    p[4] = np.array((-sx, sy, -sz))

    # print(p)

    t = np.zeros((6, 3), dtype=np.int32)
    t[0] = np.array((2, 1, 0), dtype=np.int32)
    t[1] = np.array((3, 2, 0), dtype=np.int32)
    t[2] = np.array((4, 3, 0), dtype=np.int32)
    t[3] = np.array((1, 4, 0), dtype=np.int32)
    t[4] = np.array((2, 3, 1), dtype=np.int32)
    t[5] = np.array((4, 1, 3), dtype=np.int32)

    # print(t)
    return p, t


# Z+向きのカメラのメッシュ vertex listとtriangle list。
def Generate_CameraMeshZP(scale=0.1):
    # Z-向きのメッシュを作った後Y軸で180度回転する。
    p, t = Generate_CameraMeshZM(scale)
    p = Transform_PointList(p, RotY(math.pi))
    return p, t


# メッシュをPLYで保存。
def ExportMesh_Ply(path: str, p, v):
    ps = p.shape
    vs = v.shape

    with open(path, "w", newline="\n") as f:
        f.write("ply\n")
        f.write("format ascii 1.0\n")

        f.write(f"element vertex {ps[0]}\n")
        f.write("property float x\n")
        f.write("property float y\n")
        f.write("property float z\n")

        f.write(f"element face {vs[0]}\n")
        f.write("property list uchar uint vertex_indices\n")
        f.write("end_header\n")

        for i in range(ps[0]):
            f.write(f"{p[i,0]} {p[i,1]} {p[i,2]}\n")

        for i in range(vs[0]):
            f.write(f"3 {int(v[i,0])} {int(v[i,1])} {int(v[i,2])}\n")


# 3次元点群をPLYで保存。
def PLY_Export_PointNdArray(path: str, p: np.ndarray):
    print(f"PLY__Export_PointList {path}")

    with open(path, "w", newline="\n") as f:
        f.write("ply\n")
        f.write("format ascii 1.0\n")

        ps = p.shape
        f.write(f"element vertex {ps[0]}\n")
        f.write("property float x\n")
        f.write("property float y\n")
        f.write("property float z\n")
        f.write("end_header\n")

        for i in range(ps[0]):
            f.write(f"{p[i,0]} {p[i,1]} {p[i,2]}\n")


def PLY_Export_PointList(path: str, P_list: list):
    print(f"PLY__Export_PointList {path}")

    with open(path, "w", newline="\n") as f:
        f.write("ply\n")
        f.write("format ascii 1.0\n")

        nPoints = len(P_list)
        f.write(f"element vertex {nPoints}\n")
        f.write("property float x\n")
        f.write("property float y\n")
        f.write("property float z\n")
        f.write("end_header\n")

        for i in range(nPoints):
            p = P_list[i].flatten()
            f.write(f"{p[0]} {p[1]} {p[2]}\n")


# 色付き3次元点群をPLYで保存。
def ExportColoredPoints_Ply(path: str, p, c_list):
    ps = p.shape

    with open(path, "w", newline="\n") as f:
        f.write("ply\n")
        f.write("format ascii 1.0\n")

        f.write(f"element vertex {ps[0]}\n")
        f.write("property float x\n")
        f.write("property float y\n")
        f.write("property float z\n")
        f.write("property int r\n")
        f.write("property int g\n")
        f.write("property int b\n")
        f.write("end_header\n")

        for i in range(ps[0]):
            c = c_list[i]
            r = 0
            g = 0
            b = 255
            if c == 0:
                r = 1
                g = 0
                b = 0
            f.write(f"{p[i,0]} {p[i,1]} {p[i,2]} {r} {g} {b}\n")


# 4x4行列Mに3次元座標p各点の縦ベクトルを右から掛ける。
# 変換後の3次元座標が戻ります。
def Transform_PointList(p, M):
    r = np.zeros(p.shape)
    for i in range(p.shape[0]):
        v = np.vstack([p[i, 0], p[i, 1], p[i, 2], 1])
        v = M @ v
        v /= v[3, 0]
        r[i, 0:3] = np.hstack(v)[0:3]

    return r


# 4x4行列Mに3次元座標p各点の縦ベクトルを右から掛ける。
# プロジェクション変換後の2次元座標が戻ります。
def Project_PointList(p, M):
    ps = p.shape
    r = []

    for i in range(p.shape[0]):
        v = np.vstack([p[i, 0], p[i, 1], p[i, 2], 1])
        v = M @ v
        v /= v[3, 0]
        if 0 < v[2, 0]:
            r.append(np.hstack(v)[0:3])

    return np.array(r)


def MergeMesh(p0, v0, p1, v1):
    p0s = p0.shape
    p1s = p1.shape

    v0s = v0.shape
    v1s = v1.shape

    # v1の頂点番号をずらします。
    v1a = np.zeros(v1.shape, dtype=np.int32)
    for i in range(v1s[0]):
        v1a[i, :] = v1[i, :] + p0s[0]

    p = np.append(p0, p1).reshape(p0s[0] + p1s[0], p0s[1])
    v = np.append(v0, v1a).reshape(v0s[0] + v1s[0], v0s[1])

    return p, v


def cross_operator(t):
    """
    p.79 eq 5.10
    """
    tf = t.flatten()
    t1 = tf[0]
    t2 = tf[1]
    t3 = tf[2]

    return np.array([[0, -t3, t2], [t3, 0, -t1], [-t2, t1, 0]])


def Reconstruct_F_from(t, R, f0, focalLen_Cam0, focalLen_Cam1):
    f0_f0_c0 = np.array([[f0, 0, 0], [0, f0, 0], [0, 0, focalLen_Cam0]])
    f0_f0_c1 = np.array([[f0, 0, 0], [0, f0, 0], [0, 0, focalLen_Cam1]])
    t_x_R = cross_operator(t) @ R
    recon_F = f0_f0_c0 @ t_x_R @ f0_f0_c1

    recon_F *= 1.0 / recon_F[2, 2]
    return recon_F


def Trans_Rot_to_TransformMat(t, R):
    """
    2台目のカメラの姿勢行列を戻す。
    p.77 図5.2参照。
    """

    t = t.flatten()

    t4 = np.array(
        [
            [1, 0, 0, -t[0]],
            [0, 1, 0, -t[1]],
            [0, 0, 1, -t[2]],
            [0, 0, 0, 1],
        ]
    )

    R4 = np.array(
        [
            [R[0, 0], R[0, 1], R[0, 2], 0],
            [R[1, 0], R[1, 1], R[1, 2], 0],
            [R[2, 0], R[2, 1], R[2, 2], 0],
            [0, 0, 0, 1],
        ]
    )

    cam0_to_cam1 = t4 @ R4

    return cam0_to_cam1


# 2つのカメラの関係図のPLYファイルを出力。
# カメラはOpenGL様式：Z-向き。
# カメラ姿勢M0, M1
def GeneratePLY_TwoCamPoseZM(path: str, M0, M1):
    p, v = Generate_CameraMeshZM()

    p0 = Transform_PointList(p, M0)
    p1 = Transform_PointList(p, M1)

    pW, vW = MergeMesh(p0, v, p1, v)
    ExportMesh_Ply(path, pW, vW)


# 2つのカメラの関係図のPLYファイルを出力。
# カメラはZ+向き。
# カメラ姿勢M0, M1
def GeneratePLY_TwoCamPoseZP(path: str, M0, M1):
    p, v = Generate_CameraMeshZP()

    p0 = Transform_PointList(p, M0)
    p1 = Transform_PointList(p, M1)

    pW, vW = MergeMesh(p0, v, p1, v)
    ExportMesh_Ply(path, pW, vW)


def GeneratePLY_MultiCamPoseZP(path: str, M_list):
    p, v = Generate_CameraMeshZP()

    pW = Transform_PointList(p, M_list[0])
    vW = v
    for k in range(1, len(M_list)):
        M = M_list[k]
        pk = Transform_PointList(p, M)
        pW, vW = MergeMesh(pW, vW, pk, v)

    ExportMesh_Ply(path, pW, vW)


def PLY_Export_TwoCam(path: str, t, R):
    E4 = np.eye(4)
    M = Trans_Rot_to_TransformMat(t, R)

    GeneratePLY_TwoCamPoseZP(path, E4, M)


def Trans_Rot_to_CameraPoseMat(t, R):
    # 回転してから平行移動する。
    M = np.array(
        [
            [R[0, 0], R[0, 1], R[0, 2], t[0, 0]],
            [R[1, 0], R[1, 1], R[1, 2], t[1, 0]],
            [R[2, 0], R[2, 1], R[2, 2], t[2, 0]],
            [0, 0, 0, 1],
        ]
    )
    return M


def PLY_Export_MultiCam(path: str, t_list, R_list):
    M_list = []
    for k in range(len(t_list)):
        t = t_list[k]
        R = R_list[k]

        # M = Trans_Rot_to_TransformMat(t, R)
        M = Trans_Rot_to_CameraPoseMat(t, R)

        M_list.append(M)

    GeneratePLY_MultiCamPoseZP(path, M_list)


# 楕円用。
def BuildXi_ellip(x_list, y_list, f0):
    N = x_list.shape[0]
    assert N == y_list.shape[0]

    xi_list = []
    for i in range(N):
        x = x_list[i]
        y = y_list[i]

        xi = np.vstack([x * x, 2 * x * y, y * y, 2 * f0 * x, 2 * f0 * y, f0 * f0])
        xi_list.append(xi)
    return xi_list


# 楕円用。
def BuildV0_ellip(x_list, y_list, f0):
    N = x_list.shape[0]
    assert N == y_list.shape[0]

    v0_list = []
    for i in range(N):
        x = x_list[i]
        y = y_list[i]

        v0 = np.zeros((6, 6))
        v0[0, 0] = 4 * x * x
        v0[0, 1] = v0[1, 0] = v0[1, 2] = v0[2, 1] = 4 * x * y
        v0[1, 1] = 4 * (x * x + y * y)
        v0[2, 2] = 4 * y * y
        v0[0, 3] = v0[3, 0] = v0[1, 4] = v0[4, 1] = 4 * f0 * x
        v0[1, 3] = v0[3, 1] = v0[2, 4] = v0[4, 2] = 4 * f0 * y
        v0[3, 3] = v0[4, 4] = 4 * f0 * f0
        v0_list.append(v0)
    return v0_list


# ベクトルの定数係数を1にする。
def vec9_scale_const_unit(a):
    a /= a[8, 0]
    return a


# 2台カメラ用。
# ch2. p14. eq2.15
# xi fundamental mat
def BuildXi_F(pp: Point2dPair, f0):
    N = pp.get_point_count()

    xi_list = []
    for i in range(N):
        x0 = pp.a[i, 0]
        y0 = pp.a[i, 1]
        x1 = pp.b[i, 0]
        y1 = pp.b[i, 1]

        xi = np.vstack(
            [
                x0 * x1,
                x0 * y1,
                f0 * x0,
                x1 * y0,
                y0 * y1,
                f0 * y0,
                f0 * x1,
                f0 * y1,
                f0 * f0,
            ],
            dtype=np.float64,
        )
        xi_list.append(xi)
    return xi_list


# 2台カメラ用。
# V0 of Fundamental mat. ch3. p38. eq3.12.
def BuildV0_F1(xy0, xy1, f0):
    x0 = xy0[0]
    y0 = xy0[1]
    x1 = xy1[0]
    y1 = xy1[1]
    v0 = np.zeros((9, 9), dtype=np.float64)
    v0[0, 0] = x0 * x0 + x1 * x1
    v0[1, 1] = x0 * x0 + y1 * y1
    v0[3, 3] = x1 * x1 + y0 * y0
    v0[4, 4] = y0 * y0 + y1 * y1
    v0[3, 0] = v0[0, 3] = v0[4, 1] = v0[1, 4] = x0 * y0
    v0[0, 1] = v0[1, 0] = v0[3, 4] = v0[4, 3] = x1 * y1
    v0[2, 0] = v0[0, 2] = v0[5, 3] = v0[3, 5] = f0 * x1
    v0[2, 1] = v0[1, 2] = v0[5, 4] = v0[4, 5] = f0 * y1
    v0[2, 2] = v0[5, 5] = v0[6, 6] = v0[7, 7] = f0 * f0
    v0[6, 0] = v0[0, 6] = v0[7, 1] = v0[1, 7] = f0 * x0
    v0[6, 3] = v0[3, 6] = v0[7, 4] = v0[4, 7] = f0 * y0
    return v0


# 2台カメラ用。
# V0 of Fundamental mat. ch3. p38. eq3.12.
def BuildV0_F(pp: Point2dPair, f0):
    N = pp.get_point_count()

    v0_list = []
    for i in range(N):
        xy0 = pp.a[i, :]
        xy1 = pp.b[i, :]
        v0 = BuildV0_F1(xy0, xy1, f0)
        v0_list.append(v0)
    return v0_list


# procedure 3,4 eq.3.30 p.43
# θ→θ†
def ThetaDagger(t):
    t1 = t[0, 0]
    t2 = t[1, 0]
    t3 = t[2, 0]
    t4 = t[3, 0]
    t5 = t[4, 0]

    t6 = t[5, 0]
    t7 = t[6, 0]
    t8 = t[7, 0]
    t9 = t[8, 0]

    thetaD = np.vstack(
        [
            t5 * t9 - t8 * t6,
            t6 * t7 - t9 * t4,
            t4 * t8 - t7 * t5,
            t8 * t3 - t2 * t9,
            t9 * t1 - t3 * t7,
            t7 * t2 - t1 * t8,
            t2 * t6 - t5 * t3,
            t3 * t4 - t6 * t1,
            t1 * t5 - t4 * t2,
        ]
    )
    return thetaD


# θ→F. eq.3.3右 p.36
def ThetaToF(t):
    # t は長さ9の1次元配列、または 9x1 の列ベクトルのどちらも受け付ける
    t = np.asarray(t).reshape(9)
    F = np.zeros((3, 3))
    F[0, 0] = t[0]
    F[0, 1] = t[1]
    F[0, 2] = t[2]

    F[1, 0] = t[3]
    F[1, 1] = t[4]
    F[1, 2] = t[5]

    F[2, 0] = t[6]
    F[2, 1] = t[7]
    F[2, 2] = t[8]
    return F


def FToTheta(F):
    t = np.asarray(F).reshape(9)
    t = np.vstack(t)

    return t


def Update_W(theta, v0_list):
    N = len(v0_list)
    w_list = np.asarray(N * [1.0])

    for i in range(N):
        v0 = v0_list[i]
        v0ev = v0 @ theta
        w_list[i] = 1.0 / (theta.T @ v0ev).item()  # 1x1 mat to number

    return w_list


# ベクトルの大きさを1にスケール。
def vec9_normalize(a):
    a /= np.linalg.norm(a)
    if a[8, 0] < 0:
        a = -a
    return a


# 固有ベクトルを縦ベクトルにして、単位ベクトルになるようスケール
def reshape_ev_norm(ev):
    theta = ev.reshape(9)
    theta = np.vstack(theta)
    return vec9_normalize(theta)


# 固有ベクトルを縦ベクトルにして、定数項が1になるようスケール
def reshape_ev_const_unit(ev):
    theta = ev.reshape(9)
    theta = np.vstack(theta)
    return vec9_scale_const_unit(theta)


# 最小二乗法で最適解を求めます。 手順3.1 p.38
def TwoCam_LeastSquare(pp: Point2dPair, f0):
    xi_list = BuildXi_F(pp, f0)

    M = BuildM_LSQ(xi_list)
    # Mの最小固有値に対応する固有ベクトルthetaを得る。
    _, eig_vec = eigh(M)
    theta = reshape_ev_const_unit(eig_vec[:, 0])
    return theta


# Taubin法で基礎行列を求めます。 手順3.2 p.39
def TwoCam_Taubin(pp: Point2dPair, f0):
    xi_list = BuildXi_F(pp, f0)
    n = len(xi_list)
    M = BuildM_LSQ(xi_list)

    V0xi_list = BuildV0_F(pp, f0)
    N = np.zeros((9, 9), dtype=np.float64)
    for V0xi in V0xi_list:
        N = N + V0xi
    N /= n

    print(f"M={M}")
    print(f"N={N}")

    eigval_list, eigvec_list = scipy_linalg.eigh(N, M)
    theta = eigvec_list[:, 8]
    theta = reshape_ev_norm(theta)
    return theta


# θ† → Pθ†
def PThetaDagger(td):
    I9 = np.eye(9)
    return I9 - td @ np.transpose(td) / np.vdot(td, td)


# 2台カメラ用。
# modified FNS method. ch 3.7. p49. procedure 3.6
def TwoCam_FNS(pp: Point2dPair, f0, convEPS, maxIter):
    if True:
        theta = TwoCam_LeastSquare(pp, f0)
    else:
        theta = TwoCam_Taubin(pp, f0)

    theta = vec9_normalize(theta)
    if not np.all(np.isfinite(theta)):
        return None
    xi_list = BuildXi_F(pp, f0)
    v0_list = BuildV0_F(pp, f0)
    N = len(xi_list)

    for i in range(maxIter):
        M = BuildM_ModFNS(xi_list, v0_list, theta)
        L = BuildL_ModFNS(xi_list, v0_list, theta)
        X = M - L

        # NaN/Inf が生じた場合は数値的不安定として安全に失敗する
        if not np.all(np.isfinite(X)):
            return None

        td = ThetaDagger(theta)
        if not np.all(np.isfinite(td)):
            return None
        Ptd = PThetaDagger(td)

        Y = Ptd @ X @ Ptd
        if not np.all(np.isfinite(Y)):
            return None
        eig_val, eig_vec = eigh(Y)
        v1 = reshape_ev_norm(eig_vec[:, 0])
        v2 = reshape_ev_norm(eig_vec[:, 1])
        t_hat = np.vdot(theta, v1) * v1 + np.vdot(theta, v2) * v2
        t_p = Ptd @ t_hat
        t_p = vec9_normalize(t_p)

        diff = np.linalg.norm(theta - t_p)
        # print(f"TwoCam_FNS i={i}, diff={diff}, eig_val={eig_val}")
        if diff < convEPS:
            # print(f"TwoCam_FNS i={i}, converged")
            return vec9_scale_const_unit(theta)

        theta = vec9_normalize(theta + t_p)
        if not np.all(np.isfinite(theta)):
            return None
    return None


def Epipolar_Constraint_Error(
    pp: Point2dPair, inlier_flag_list, focal_len_cam0, focal_len_cam1, F
):
    """
    Epipolar constraint errorを計算。
    """

    N = pp.get_point_count()

    valid_count = 0
    err_sum = 0.0

    for i in range(N):
        if inlier_flag_list[i] == False:
            continue
        x0 = pp.a[i, 0]
        y0 = pp.a[i, 1]
        x1 = pp.b[i, 0]
        y1 = pp.b[i, 1]

        xy0 = np.vstack([x0 / focal_len_cam0, y0 / focal_len_cam0, 1])

        xy1 = np.vstack([x1 / focal_len_cam1, y1 / focal_len_cam1, 1])

        Fxy1 = np.matmul(F, xy1)

        # print(f"xy0={xy0}")
        # print(f"Fxy1={Fxy1}")

        err = np.vdot(xy0, Fxy1)

        err_sum += err
        valid_count += 1

    return err_sum / valid_count, valid_count


#                       N
# 対称行列 M = (1/N) * Σ xi_i * xi_i^T
#                      i=0
# 最小二乗法。
def BuildM_LSQ(xi_list):
    N = len(xi_list)

    C = xi_list[0].shape[0]

    M = np.zeros((C, C), dtype=np.float64)
    for i in range(N):
        xi = xi_list[i]

        a = (1.0 / N) * xi @ xi.T
        M += a
    return M


#                       N
# 対称行列 M = (1/N) * Σw_i * xi_i * xi_i^T
#                      i=0
# FNS法。
def BuildM_FNS(xi_list, w_list):
    N = len(xi_list)
    assert N == w_list.shape[0]

    C = xi_list[0].shape[0]

    M = np.zeros((C, C))
    for i in range(N):
        xi = xi_list[i]
        w = w_list[i]

        a = (w / N) * xi @ xi.T
        M += a
    return M


# 拡張FNS法のM。
def BuildM_ModFNS(xi_list, v0_list, theta):
    N = len(xi_list)
    assert N == len(v0_list)

    C = xi_list[0].shape[0]

    M = np.zeros((C, C))
    for i in range(N):
        xi = xi_list[i]
        v0 = v0_list[i]

        numer = xi @ np.transpose(xi)
        denom = np.vdot(theta, v0 @ theta)

        a = (1 / N) * numer / denom
        M += a
    return M


# 対称行列L
# FNS法。
def BuildL_FNS(xi_list, w_list, v0_list, theta):
    N = len(xi_list)
    assert N == w_list.shape[0]
    assert N == len(v0_list)

    C = xi_list[0].shape[0]

    L = np.zeros((C, C))
    for i in range(N):
        xi = xi_list[i]
        w = w_list[i]
        v0 = v0_list[i]
        xi_dot_theta = (xi.T @ theta).item()

        a = (w * w / N) * xi_dot_theta * xi_dot_theta * v0
        L += a
    return L


# 拡張FNS法のL。
def BuildL_ModFNS(xi_list, v0_list, theta):
    N = len(xi_list)
    assert N == len(v0_list)

    C = xi_list[0].shape[0]

    L = np.zeros((C, C))
    for i in range(N):
        xi = xi_list[i]
        v0 = v0_list[i]

        xi_dot_theta = np.vdot(xi, theta).item()

        t_dot_v0t = np.vdot(theta, v0 @ theta)

        a = (1 / N) * xi_dot_theta * xi_dot_theta * v0 / t_dot_v0t / t_dot_v0t
        L += a
    return L


def CSV_Read_F(path: str):

    with open(path) as f:
        r = csv.reader(f, delimiter=",")
        for l in r:
            # ヘッダ行 (先頭が数値でない行) や空行はスキップする
            if len(l) < 9:
                continue
            try:
                f00 = float(l[0])
                f01 = float(l[1])
                f02 = float(l[2])
                f10 = float(l[3])
                f11 = float(l[4])
                f12 = float(l[5])
                f20 = float(l[6])
                f21 = float(l[7])
                f22 = float(l[8])
            except (ValueError, TypeError):
                continue
            return np.array([[f00, f01, f02], [f10, f11, f12], [f20, f21, f22]])


def CSV_Read_TwoCam_MatchedPointList(path: str):
    # print(f"ReadTwoCamPoints({path})")
    xy0_list = []
    xy1_list = []
    with open(path) as f:
        r = csv.reader(f, delimiter=",")
        for l in r:
            # ヘッダ行 (先頭が数値でない行) や空行はスキップする
            if len(l) < 4:
                continue
            try:
                x0 = float(l[0])
                y0 = float(l[1])
                x1 = float(l[2])
                y1 = float(l[3])
            except (ValueError, TypeError):
                continue
            xy0_list.append(np.array([x0, y0]))
            xy1_list.append(np.array([x1, y1]))
    return Point2dPair(np.asarray(xy0_list), np.asarray(xy1_list))


def CSV_Write_TwoCam_MatchedPointList(path: str, xy_pair, shift_xy=None):
    """
    対応点ペアリストを
    画素座標(x+→, y+↓)、単位ピクセル、画像左上が原点
    CSV保存します。

    shift != Noneのとき、座標を平行移動します。

    引数:
        path: 出力CSVファイルのパス
        xy_pair: 対応点ペアのリスト
                 [[ (x1,y1), (x2,y2), Lowe's ratio, reproj_error ], ...]
        img1_shape: 1枚目の画像の形状 (高さ, 幅)
        shift: Trueのとき画像座標を画面中心が原点になるようずらす。

    出力形式: 先頭にタイトル行 "x1, y1, x2, y2, Lowes_ratio, reproj_error_px" を書き出し、
              以降に1対応点ずつ "x1, y1, x2, y2, Lowe's ratio, reproj error" を書き出す

    """

    print(f"CSV_Write_TwoCam_MatchedPointList_Shifted write {path}")

    if shift_xy is not None:
        sx = shift_xy[0]
        sy = shift_xy[1]
    else:
        sx = 0
        sy = 0

    # 座標系は、x+→, y+↓、画面中心が原点。
    with open(path, "w", newline="\n") as f:
        f.write("x1(right+), y1(down+), x2, y2, Lowes_ratio, reproj_error_px\n")
        for p in xy_pair:
            p0 = p[0]
            p1 = p[1]

            x0N = p0[0] + sx
            y0N = p0[1] + sy
            x1N = p1[0] + sx
            y1N = p1[1] + sy

            f.write(f"{x0N}, {y0N}, {x1N}, {y1N}, {p[2]}, {p[3]}\n")


def PointPairList_Shift(pp_list, shift_xy):
    """
    pp_listの座標ペアを、shift_xyだけ、平行移動したものを作って戻します。
    引数のpp_listは破壊しません。
    """

    N = pp_list.N

    r0_list = []
    r1_list = []

    sx = shift_xy[0]
    sy = shift_xy[1]
    for i in range(N):
        xy0 = pp_list.a[i]
        x0 = xy0[0] + sx
        y0 = xy0[1] + sy

        xy1 = pp_list.b[i]
        x1 = xy1[0] + sx
        y1 = xy1[0] + sy

        r0_list.append(np.array([x0, y0]))
        r1_list.append(np.array([x1, y1]))
    return Point2dPair(np.asarray(r0_list), np.asarray(r1_list))


def CSV_Write_CamPose_list(path: str, t_list, R_list, cam_id_list=None):
    with open(path, "w", newline="\n") as f:
        # t: 1行3列 列ベクトル
        # R: 3行3列 回転ベクトル

        if cam_id_list is None:
            f.write("tX, tY, tZ, r00, r01, r02, r10, r11, r12, r20, r21, r22\n")
        else:
            f.write("camera_id, tX, tY, tZ, r00, r01, r02, r10, r11, r12, r20, r21, r22\n")

        for k in range(len(t_list)):
            t = t_list[k]
            R = R_list[k]
            if cam_id_list is None:
                f.write(f"{t[0,0]}, {t[1,0]}, {t[2,0]}, ")
            else:
                f.write(f"{cam_id_list[k]}, {t[0,0]}, {t[1,0]}, {t[2,0]}, ")
            f.write(f"{R[0,0]}, {R[0,1]}, {R[0,2]}, ")
            f.write(f"{R[1,0]}, {R[1,1]}, {R[1,2]}, ")
            f.write(f"{R[2,0]}, {R[2,1]}, {R[2,2]}\n")


def CSV_Write_CamPose(path: str, t, R):
    CSV_Write_CamPose_list(path, [t], [R])


def CSV_Write_Point3d_list(path: str, p_list):
    with open(path, "w", newline="\n") as f:
        f.write("X, Y, Z\n")
        for p in p_list:
            xyz = p.flatten()
            f.write(f"{xyz[0]}, {xyz[1]}, {xyz[2]}\n")


def CSV_Write_TwoCamFocalLengths(path: str, FL0, FL1):
    # print(f"Writing two cam focal lengths to {path}...")

    with open(path, "w", newline="\n") as f:
        f.write(f"{FL0}, {FL1}\n")


def CSV_Write_MatchedPointList3(path: str, xyz_triplet, shift_xy, cam_id_list):
    sx = shift_xy[0]
    sy = shift_xy[1]

    # 座標系は、x+→, y+↓
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="\n") as f:
        f.write("cam_id_A, xA, yA, cam_id_B, xB, yB, cam_id_C, xC, yC\n")
        for p in xyz_triplet:
            c0 = cam_id_list[0]
            c1 = cam_id_list[1]
            c2 = cam_id_list[2]
            p0 = p[0]
            p1 = p[1]
            p2 = p[2]
            p0 = ((p0[0] + sx), (p0[1] + sy))
            p1 = ((p1[0] + sx), (p1[1] + sy))
            p2 = ((p2[0] + sx), (p2[1] + sy))

            f.write(
                f"{c0}, {p0[0]}, {p0[1]}, {c1}, {p1[0]}, {p1[1]}, {c2}, {p2[0]}, {p2[1]}\n"
            )


class FeaturePoint:
    """
    pointId番の特徴点1個の情報を保持。

    カメラはM個(CameraCount()個)あり、
    カメラ番号kは0から連番で振られる。

    各カメラから見える特徴点の座標(x,y)を保持。
    """

    def __init__(self, pointId, xy_list, f0):
        self.pointId = pointId
        self.xy_list = xy_list
        self.f0 = f0

    def CameraCount(self):
        return len(self.xy_list)

    def FeaturePointCoordOfCam(self, i):
        """
        i番カメラの画像に写った特徴点の座標(x,y)を戻す。
        """
        return self.xy_list[i]

    def x_ak(self, k):
        """
        p.201 eq.13.11
        カメラ番号kのx_ak行列を戻す。
        x_akは3行1列の行列。
        """
        xy = self.xy_list[k]
        f0 = self.f0
        return np.array([[xy[0] / f0], [xy[1] / f0], [1]])


class CamId_x_y:
    def __init__(self, camId, x, y):
        self.camId = camId
        self.x = x
        self.y = y

    def CamId(self):
        return self.camId

    def X(self):
        return self.x

    def Y(self):
        return self.y


def CSV_Read_FeaturePointList(csv_path: str, f0):
    """
    CSV_Write_MatchedPointList3が保存したCSVファイルを読み、
    FeaturePointのlistを作る。

    cam_id_A, xA, yA, cam_id_B, xB, yB, cam_id_C, xC, yC
    """
    N_CAMS = 3

    camId_list = []

    fp_list = []
    with open(csv_path) as f:
        r = csv.reader(f, delimiter=",")
        for b in r:
            # ヘッダ行 (先頭が数値でない行) や空行はスキップする
            if len(b) < N_CAMS * 3:
                continue

            xy_list = []
            camIdxy_list = []
            try:
                idx = 0
                for i in range(N_CAMS):
                    camId = int(b[idx + 0])
                    x = float(b[idx + 1])
                    y = float(b[idx + 2])

                    camIdxy_list.append(CamId_x_y(camId, x, y))
                    xy_list.append(np.array([x, y]))
                    idx += 3
            except (ValueError, TypeError):
                continue

            if len(camId_list) == 0:
                # 最初の行。
                for i in range(N_CAMS):
                    camId_list.append(camIdxy_list[i].CamId())
            else:
                # 2行目以降は、カメラIDが変わらないことを確認。
                for i in range(N_CAMS):
                    if camId_list[i] != camIdxy_list[i].CamId():
                        print(
                            f"Error: camID changed unexpectedly on line {r.line_num}. {camId_list[i]} {camIdxy_list[i].CamId()}"
                        )

            pointId = len(fp_list)
            fp_list.append(FeaturePoint(pointId, xy_list, f0))

    return fp_list


def ReadPointXY3(path: str):
    x_list = []
    y_list = []
    with open(path) as f:
        r = csv.reader(f, quoting=csv.QUOTE_NONNUMERIC, delimiter=",")
        for l in r:
            x_list.append(l[0])
            y_list.append(l[1])
    return np.asarray(x_list), np.asarray(y_list)


# フィットした楕円を図示。
def Plot(ev, w_list, f0, x_list, y_list):
    # 楕円方程式の係数。
    # Ax^2 + 2Bxy + Cy^2 + 2f0(Dx + Ey) + f0^2*F = 0

    ev = ev.reshape(6)
    A = ev[0]
    B = ev[1]
    C = ev[2]
    D = ev[3]
    E = ev[4]
    F = ev[5]

    print(f"{A}x^2 + {2*B}xy + {C}y^2 + {2*f0*D}x + {2*f0*E}y + {f0*f0*F} = 0")

    # テスト
    # A=1
    # B=0
    # C=1
    # D=0
    # E=0
    # F=-10
    # x_min=-10
    # x_max=10

    x_min = np.min(x_list)
    x_max = np.max(x_list)
    y_min = np.min(y_list)
    y_max = np.max(y_list)

    # 楕円をプロットするために楕円上の点を集める。
    ex_list = []
    ey_list = []

    for x in np.arange(x_min - 10, x_max + 10, (x_max - x_min) / 100):
        det = ((2 * E * f0 + 2 * B * x) ** 2) - 4 * C * (
            F * f0 * f0 + 2 * D * f0 * x + A * x * x
        )
        if 0 <= det:
            yP = (-2 * E * f0 - 2 * B * x + math.sqrt(det)) / (2 * C)
            yM = (-2 * E * f0 - 2 * B * x - math.sqrt(det)) / (2 * C)
            ex_list.append(x)
            ey_list.append(yP)
            ex_list.append(x)
            ey_list.append(yM)

    for y in np.arange(y_min - 10, y_max + 10, (y_max - y_min) / 100):
        det = ((2 * D * f0 + 2 * B * y) ** 2) - 4 * A * (
            F * f0 * f0 + 2 * E * f0 * y + C * y * y
        )
        if 0 <= det:
            xP = (-2 * D * f0 - 2 * B * y + math.sqrt(det)) / (2 * A)
            xM = (-2 * D * f0 - 2 * B * y - math.sqrt(det)) / (2 * A)
            ex_list.append(xP)
            ey_list.append(y)
            ex_list.append(xM)
            ey_list.append(y)

    plt.scatter(ex_list, ey_list, s=5, marker="x", c="black")
    plt.scatter(x_list, y_list, s=1, c=w_list, marker=".", cmap="bwr")
    plt.axis("equal")
    plt.colorbar()
    plt.title("Ransac FNS")
    plt.show()


# adjust xy0, xy1 point to intersect
def AdjustTwoPoints1(xy0, xy1, theta, f0):
    S0 = math.inf
    S = 0
    xyh0 = np.array(xy0)
    xyh1 = np.array(xy1)
    xyt0 = np.array([0, 0])
    xyt1 = np.array([0, 0])

    while np.abs(S - S0) > 1:
        S0 = S
        V0 = BuildV0_F1(xy0, xy1, f0)
        xi_star = np.vstack(
            [
                xyh0[0] * xyh1[0] + xyh1[0] * xyt0[0] + xyh0[0] * xyt1[0],
                xyh0[0] * xyh1[1] + xyh1[1] * xyt0[0] + xyh0[0] * xyt1[1],
                f0 * (xyh0[0] + xyt0[0]),
                xyh0[1] * xyh1[0] + xyh1[0] * xyt0[1] + xyh0[1] * xyt1[0],
                xyh0[1] * xyh1[1] + xyh1[1] * xyt0[1] + xyh0[1] * xyt1[1],
                f0 * (xyh0[1] + xyt0[1]),
                f0 * (xyh1[0] + xyt1[0]),
                f0 * (xyh1[1] + xyt1[1]),
                f0 * f0,
            ]
        )

        t123 = np.array(
            [[theta[0], theta[1], theta[2]], [theta[3], theta[4], theta[5]]]
        ).reshape(2, 3)
        t147 = np.array(
            [[theta[0], theta[3], theta[6]], [theta[1], theta[4], theta[7]]]
        ).reshape(2, 3)

        xyfh0 = np.vstack([xyh0[0], xyh0[1], f0])
        xyfh1 = np.vstack([xyh1[0], xyh1[1], f0])
        s = np.vdot(xi_star, theta) / np.vdot(theta, V0 @ theta)
        xyt0 = (s * (t123 @ xyfh1)).reshape(2)
        xyt1 = (s * (t147 @ xyfh0)).reshape(2)

        xyh0 = xy0 - xyt0
        xyh1 = xy1 - xyt1

        S = (
            xyt0[0] * xyt0[0]
            + xyt0[1] * xyt0[1]
            + xyt1[0] * xyt1[0]
            + xyt1[1] * xyt1[1]
        )

    return xyh0, xyh1


def AdjustTwoPoints(pp: Point2dPair, valid_bitmap, theta, f0):
    """
    p.69 手順4.2 対応点の最適補正。
    """

    N = pp.get_point_count()

    for i in range(N):
        if valid_bitmap[i] == False:
            continue
        pp.a[i, :], pp.b[i, :] = AdjustTwoPoints1(pp.a[i, :], pp.b[i, :], theta, f0)

    return pp


def Triangulation(
    pp: Point2dPair, valid_bitmap, f0, P0, P1, reject_behind_points=False
):
    """
    triangulation ch4 p66 eq4.1


    """
    N = pp.get_point_count()

    xyz_list = np.ndarray((N, 3))
    z_sign = 0

    for i in range(N):
        inlier = valid_bitmap[i]
        if not inlier:
            continue

        xy0 = pp.a[i, :]
        xy1 = pp.b[i, :]

        T = np.array(
            [
                [
                    f0 * P0[0, 0] - xy0[0] * P0[2, 0],
                    f0 * P0[0, 1] - xy0[0] * P0[2, 1],
                    f0 * P0[0, 2] - xy0[0] * P0[2, 2],
                ],
                [
                    f0 * P0[1, 0] - xy0[1] * P0[2, 0],
                    f0 * P0[1, 1] - xy0[1] * P0[2, 1],
                    f0 * P0[1, 2] - xy0[1] * P0[2, 2],
                ],
                [
                    f0 * P1[0, 0] - xy1[0] * P1[2, 0],
                    f0 * P1[0, 1] - xy1[0] * P1[2, 1],
                    f0 * P1[0, 2] - xy1[0] * P1[2, 2],
                ],
                [
                    f0 * P1[1, 0] - xy1[1] * P1[2, 0],
                    f0 * P1[1, 1] - xy1[1] * P1[2, 1],
                    f0 * P1[1, 2] - xy1[1] * P1[2, 2],
                ],
            ]
        )

        p = np.vstack(
            [
                f0 * P0[0, 3] - xy0[0] * P0[2, 3],
                f0 * P0[1, 3] - xy0[1] * P0[2, 3],
                f0 * P1[0, 3] - xy1[0] * P1[2, 3],
                f0 * P1[1, 3] - xy1[1] * P1[2, 3],
            ]
        )

        with np.errstate(all="ignore"):
            rv = np.linalg.lstsq((T.T) @ T, (T.T) @ p, rcond=None)
        xyz = rv[0].flatten()
        xyz_list[i, :] = xyz
        if 0 < xyz[0]:
            z_sign = z_sign + 1
        else:
            z_sign = z_sign - 1

    if z_sign < 0:
        for i in range(N):
            xyz_list[i, :] = -xyz_list[i, :]

    # reject outliers that is z < 0 カメラの後ろにある点。
    valid_point_count = 0
    for i in range(N):
        z = xyz_list[i, 2]
        if z < 0 and reject_behind_points:
            valid_bitmap[i] = False

        if valid_bitmap[i] == True:
            valid_point_count += 1

    return xyz_list, valid_bitmap, valid_point_count


def InlierPointList_from_bitmap(xyz_list, valid_bitmap):
    N = xyz_list.shape[0]
    xyz_list2 = []
    for i in range(N):
        if valid_bitmap[i] == False:
            continue
        xyz = xyz_list[i]
        xyz_list2.append(xyz)
    return np.array(xyz_list2)


def InlierPointList_from_inlierIdList(xyz_list, inlier_ids):
    return xyz_list[inlier_ids, :]


def PlotValidPoints(pp, loss_list):
    plt.scatter(pp.a[:, 0], pp.a[:, 1], s=3, c=loss_list, marker=".", cmap="bwr")

    # for i, l in enumerate(loss_list):
    #    v = float(l)
    #    plt.annotate(f"{i}:{v:.1f}", (xy_list[i,0], xy_list[i,1]), )

    plt.axis("equal")
    plt.colorbar()
    plt.title("Valid Points")
    plt.show()


def Plot3D(xyz_list, c_list):
    x_list = np.array(xyz_list)[:, 0]
    y_list = np.array(xyz_list)[:, 1]
    z_list = np.array(xyz_list)[:, 2]

    fig = plt.figure()

    ax = plt.axes(projection="3d")

    ax.scatter(x_list, y_list, z_list, c=c_list)

    ax.set_title("Estimated 3D points")
    plt.show()


def build_A_(Qk_list):
    """
    eq.13.41
    """
    nCams = len(Qk_list)

    A_ = np.zeros((4, 4, 4, 4))

    for a in range(4):
        for b in range(4):
            for c in range(4):
                for d in range(4):
                    s = 0.0
                    for i in range(nCams):
                        Qk = Qk_list[i]
                        s += (
                            Qk[0, a] * Qk[0, b] * Qk[0, c] * Qk[0, d]
                            - Qk[0, a] * Qk[0, b] * Qk[1, c] * Qk[1, d]
                            - Qk[1, a] * Qk[1, b] * Qk[0, c] * Qk[0, d]
                            + Qk[1, a] * Qk[1, b] * Qk[1, c] * Qk[1, d]
                            + 0.25
                            * (
                                Qk[0, a] * Qk[1, b] * Qk[0, c] * Qk[1, d]
                                + Qk[1, a] * Qk[0, b] * Qk[0, c] * Qk[1, d]
                                + Qk[0, a] * Qk[1, b] * Qk[1, c] * Qk[0, d]
                                + Qk[1, a] * Qk[0, b] * Qk[1, c] * Qk[0, d]
                            )
                            + 0.25
                            * (
                                Qk[1, a] * Qk[2, b] * Qk[1, c] * Qk[2, d]
                                + Qk[2, a] * Qk[1, b] * Qk[1, c] * Qk[2, d]
                                + Qk[1, a] * Qk[2, b] * Qk[2, c] * Qk[1, d]
                                + Qk[2, a] * Qk[1, b] * Qk[2, c] * Qk[1, d]
                            )
                            + 0.25
                            * (
                                Qk[2, a] * Qk[0, b] * Qk[2, c] * Qk[0, d]
                                + Qk[0, a] * Qk[2, b] * Qk[2, c] * Qk[0, d]
                                + Qk[2, a] * Qk[0, b] * Qk[0, c] * Qk[2, d]
                                + Qk[0, a] * Qk[2, b] * Qk[0, c] * Qk[2, d]
                            )
                        )

                    A_[a, b, c, d] = s

    return A_


def build_A(A_):
    """
    p.210 eq.13.42
    """
    sq2 = np.sqrt(2.0)
    A = np.array(
        [
            [
                A_[0, 0, 0, 0],
                A_[0, 0, 1, 1],
                A_[0, 0, 2, 2],
                A_[0, 0, 3, 3],
                sq2 * A_[0, 0, 0, 1],
                sq2 * A_[0, 0, 0, 2],
                sq2 * A_[0, 0, 0, 3],
                sq2 * A_[0, 0, 1, 2],
                sq2 * A_[0, 0, 1, 3],
                sq2 * A_[0, 0, 2, 3],
            ],
            [
                A_[1, 1, 0, 0],
                A_[1, 1, 1, 1],
                A_[1, 1, 2, 2],
                A_[1, 1, 3, 3],
                sq2 * A_[1, 1, 0, 1],
                sq2 * A_[1, 1, 0, 2],
                sq2 * A_[1, 1, 0, 3],
                sq2 * A_[1, 1, 1, 2],
                sq2 * A_[1, 1, 1, 3],
                sq2 * A_[1, 1, 2, 3],
            ],
            [
                A_[2, 2, 0, 0],
                A_[2, 2, 1, 1],
                A_[2, 2, 2, 2],
                A_[2, 2, 3, 3],
                sq2 * A_[2, 2, 0, 1],
                sq2 * A_[2, 2, 0, 2],
                sq2 * A_[2, 2, 0, 3],
                sq2 * A_[2, 2, 1, 2],
                sq2 * A_[2, 2, 1, 3],
                sq2 * A_[2, 2, 2, 3],
            ],
            [
                A_[3, 3, 0, 0],
                A_[3, 3, 1, 1],
                A_[3, 3, 2, 2],
                A_[3, 3, 3, 3],
                sq2 * A_[3, 3, 0, 1],
                sq2 * A_[3, 3, 0, 2],
                sq2 * A_[3, 3, 0, 3],
                sq2 * A_[3, 3, 1, 2],
                sq2 * A_[3, 3, 1, 3],
                sq2 * A_[3, 3, 2, 3],
            ],
            [
                sq2 * A_[0, 1, 0, 0],
                sq2 * A_[0, 1, 1, 1],
                sq2 * A_[0, 1, 2, 2],
                sq2 * A_[0, 1, 3, 3],
                2.0 * A_[0, 1, 0, 1],
                2.0 * A_[0, 1, 0, 2],
                2.0 * A_[0, 1, 0, 3],
                2.0 * A_[0, 1, 1, 2],
                2.0 * A_[0, 1, 1, 3],
                2.0 * A_[0, 1, 2, 3],
            ],
            [
                sq2 * A_[0, 2, 0, 0],
                sq2 * A_[0, 2, 1, 1],
                sq2 * A_[0, 2, 2, 2],
                sq2 * A_[0, 2, 3, 3],
                2.0 * A_[0, 2, 0, 1],
                2.0 * A_[0, 2, 0, 2],
                2.0 * A_[0, 2, 0, 3],
                2.0 * A_[0, 2, 1, 2],
                2.0 * A_[0, 2, 1, 3],
                2.0 * A_[0, 2, 2, 3],
            ],
            [
                sq2 * A_[0, 3, 0, 0],
                sq2 * A_[0, 3, 1, 1],
                sq2 * A_[0, 3, 2, 2],
                sq2 * A_[0, 3, 3, 3],
                2.0 * A_[0, 3, 0, 1],
                2.0 * A_[0, 3, 0, 2],
                2.0 * A_[0, 3, 0, 3],
                2.0 * A_[0, 3, 1, 2],
                2.0 * A_[0, 3, 1, 3],
                2.0 * A_[0, 3, 2, 3],
            ],
            [
                sq2 * A_[1, 2, 0, 0],
                sq2 * A_[1, 2, 1, 1],
                sq2 * A_[1, 2, 2, 2],
                sq2 * A_[1, 2, 3, 3],
                2.0 * A_[1, 2, 0, 1],
                2.0 * A_[1, 2, 0, 2],
                2.0 * A_[1, 2, 0, 3],
                2.0 * A_[1, 2, 1, 2],
                2.0 * A_[1, 2, 1, 3],
                2.0 * A_[1, 2, 2, 3],
            ],
            [
                sq2 * A_[1, 3, 0, 0],
                sq2 * A_[1, 3, 1, 1],
                sq2 * A_[1, 3, 2, 2],
                sq2 * A_[1, 3, 3, 3],
                2.0 * A_[1, 3, 0, 1],
                2.0 * A_[1, 3, 0, 2],
                2.0 * A_[1, 3, 0, 3],
                2.0 * A_[1, 3, 1, 2],
                2.0 * A_[1, 3, 1, 3],
                2.0 * A_[1, 3, 2, 3],
            ],
            [
                sq2 * A_[2, 3, 0, 0],
                sq2 * A_[2, 3, 1, 1],
                sq2 * A_[2, 3, 2, 2],
                sq2 * A_[2, 3, 3, 3],
                2.0 * A_[2, 3, 0, 1],
                2.0 * A_[2, 3, 0, 2],
                2.0 * A_[2, 3, 0, 3],
                2.0 * A_[2, 3, 1, 2],
                2.0 * A_[2, 3, 1, 3],
                2.0 * A_[2, 3, 2, 3],
            ],
        ]
    )

    return A


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


def normalize_each_3rows(W):
    """
    観測行列Wの3行(1カメラ分)ごとのブロックを単位長に正規化する。
    PerspectiveSelfCalibration_common.cc normalize_each_3rows 相当。
    """
    nRow = W.shape[0]

    newW = W.copy()
    for r in range(0, nRow, 3):
        nrm = np.sqrt(
            np.linalg.norm(newW[r, :]) ** 2
            + np.linalg.norm(newW[r + 1, :]) ** 2
            + np.linalg.norm(newW[r + 2, :]) ** 2
        )
        newW[r : r + 3, :] /= nrm

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
