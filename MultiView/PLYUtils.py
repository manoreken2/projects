import numpy as np
from Common import (
    Transform_PointList,
    RotX,
    Trans_Rot_to_TransformMat,
    Trans_Rot_to_CameraPoseMat,
)
import math


class Mesh:
    p_list = None
    f_list = None
    c_list = None

    def __init__(self, posit_list=None, vtxId_list=None, color_list=None):
        self.p_list = posit_list
        self.f_list = vtxId_list
        self.c_list = color_list

    def Transform(self, M):
        """
        点群self.p_listをMで回転、並進する
        """
        p = self.p_list
        r = np.zeros(p.shape)
        for i in range(p.shape[0]):
            v = np.vstack([p[i, 0], p[i, 1], p[i, 2], 1])
            v = M @ v
            v /= v[3, 0]
            r[i, 0:3] = np.hstack(v)[0:3]
        self.p_list = r
        return self

    def AppendMesh(self, mesh):
        p0 = self.p_list
        p1 = mesh.p_list
        p0s = p0.shape
        p1s = p1.shape

        f0 = self.f_list
        f1 = mesh.f_list
        f0s = f0.shape
        f1s = f1.shape

        # v1の頂点番号をずらします。
        v1a = np.zeros(f1.shape, dtype=np.int32)
        for i in range(f1s[0]):
            v1a[i, :] = f1[i, :] + p0s[0]

        self.p_list = np.append(p0, p1).reshape(p0s[0] + p1s[0], p0s[1])
        self.f_list = np.append(f0, v1a).reshape(f0s[0] + f1s[0], f0s[1])

        # 頂点色。
        c0 = self.c_list
        c1 = mesh.c_list
        if c0 is not None:
            c0s = c0.shape
            c1s = c1.shape
            self.c_list = np.append(c0, c1).reshape(c0s[0] + c1s[0], c0s[1])

        return self

    def WritePLY(self, path: str):
        """メッシュをPLYで保存。"""

        ps = self.p_list.shape

        with open(path, "w", newline="\n") as f:
            f.write("ply\n")
            f.write("format ascii 1.0\n")

            f.write(f"element vertex {ps[0]}\n")
            f.write("property float x\n")
            f.write("property float y\n")
            f.write("property float z\n")

            if self.c_list is not None:
                f.write("property uchar red\n")
                f.write("property uchar green\n")
                f.write("property uchar blue\n")

            if self.f_list is not None:
                fs = self.f_list.shape
                f.write(f"element face {fs[0]}\n")
                f.write("property list uchar uint vertex_indices\n")

            f.write("end_header\n")

            for i in range(ps[0]):
                p = self.p_list
                f.write(f"{p[i,0]} {p[i,1]} {p[i,2]}")
                if self.c_list is not None:
                    c = self.c_list
                    f.write(f" {c[i,0]} {c[i,1]} {c[i,2]}")
                f.write("\n")

            if self.f_list is not None:
                fs = self.f_list.shape
                for i in range(fs[0]):
                    v = self.f_list
                    f.write(f"3 {int(v[i,0])} {int(v[i,1])} {int(v[i,2])}\n")


def Generate_CameraMeshZM(scale=0.1):
    """
    x→、y↑、Z-向きのカメラのメッシュ作成。
    """
    sz = scale * 1.5
    sx = scale
    sy = scale * 9.0 / 16.0

    """
    上から見た図
      4(3)    1(2)
           ▽
            0
     z
     ↓ x→
    """

    """
    左から見た図
      4(1)
         ▷ 0
      3(2)
    ↑
    y   z→
    """

    p = np.zeros((5, 3))
    p[0] = np.array((0, 0, 0))
    p[1] = np.array((sx, sy, -sz))
    p[2] = np.array((sx, -sy, -sz))
    p[3] = np.array((-sx, -sy, -sz))
    p[4] = np.array((-sx, sy, -sz))

    # 左側を赤にする。
    c = np.zeros((5, 3), dtype=np.int32)
    c[0] = np.array((255, 0, 0))
    c[1] = np.array((255, 255, 255))
    c[2] = np.array((255, 255, 255))
    c[3] = np.array((255, 0, 0))
    c[4] = np.array((255, 0, 0))

    t = np.zeros((6, 3), dtype=np.int32)
    t[0] = np.array((2, 1, 0), dtype=np.int32)
    t[1] = np.array((3, 2, 0), dtype=np.int32)
    t[2] = np.array((4, 3, 0), dtype=np.int32)
    t[3] = np.array((1, 4, 0), dtype=np.int32)
    t[4] = np.array((2, 3, 1), dtype=np.int32)
    t[5] = np.array((4, 1, 3), dtype=np.int32)

    return Mesh(p, t, c)


# x→、y↓、Z+向きのカメラのメッシュ作成。
def Generate_CameraMeshZP(scale=0.1):
    # x→、y↑、Z-向きのメッシュを作った後X軸で180度回転する。
    mesh = Generate_CameraMeshZM(scale)
    mesh.Transform(RotX(math.pi))
    return mesh


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


# 色付き(c_list[k]=0のとき赤、それ以外青) 3次元点群をPLYで保存。
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


# 2つのカメラの関係図のPLYファイルを出力。
# カメラはOpenGL様式：Z-向き。
# カメラ姿勢M0, M1
def GeneratePLY_TwoCamPoseZM(path: str, M0, M1):
    m0 = Generate_CameraMeshZM().Transform(M0)
    m1 = Generate_CameraMeshZM().Transform(M1)

    m0.AppendMesh(m1)
    m0.WritePLY(path)


# 2つのカメラの関係図のPLYファイルを出力。
# カメラはZ+向き。
# カメラ姿勢M0, M1
def GeneratePLY_TwoCamPoseZP(path: str, M0, M1):
    GeneratePLY_MultiCamPoseZP(path, [M0, M1])


def GeneratePLY_MultiCamPoseZP(path: str, M_list):
    # 1台目も M_list[0] で変換する(単位行列とは限らない)。
    m0 = Generate_CameraMeshZP().Transform(M_list[0])

    for k in range(1, len(M_list)):
        M = M_list[k]
        m = Generate_CameraMeshZP().Transform(M)
        m0.AppendMesh(m)

    m0.WritePLY(path)


def PLY_Export_TwoCam(path: str, t, R):
    E4 = np.eye(4)
    M = Trans_Rot_to_TransformMat(t, R)

    GeneratePLY_TwoCamPoseZP(path, E4, M)


def PLY_Export_MultiCam(path: str, t_list, R_list):
    M_list = []
    for k in range(len(t_list)):
        t = t_list[k]
        R = R_list[k]

        # M = Trans_Rot_to_TransformMat(t, R)
        M = Trans_Rot_to_CameraPoseMat(t, R)

        M_list.append(M)

    GeneratePLY_MultiCamPoseZP(path, M_list)
