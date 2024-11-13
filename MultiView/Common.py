from threading import ThreadError
import matplotlib.pyplot as plt
import csv
import numpy as np
from numpy.linalg import eigh
import math

# 右手座標系。
# 行列の要素の並びは行優先。
# トランスフォーム行列Mとベクトルvの積は
# M * v ：縦ベクトルvを右から掛ける。

def RotX(x):
    c=math.cos(x)
    s=math.sin(x)

    # 行優先: 要素は以下の表現の通りに並ぶ。
    m = np.array([ \
        [1,  0,  0,  0], \
        [0,  c, -s,  0], \
        [0,  s,  c,  0], \
        [0,  0,  0,  1] ])
    return m

def RotY(y):
    c=math.cos(y)
    s=math.sin(y)
    m = np.array([ \
        [ c,  0,  s,  0], \
        [ 0,  1,  0,  0], \
        [-s,  0,  c,  0], \
        [ 0,  0,  0,  1] ])
    return m

def RotZ(z):
    c=math.cos(z)
    s=math.sin(z)
    m = np.array([ \
        [ c, -s,  0,  0], \
        [ s,  c,  0,  0], \
        [ 0,  0,  1,  0], \
        [ 0,  0,  0,  1] ])
    return m

# rx ry rzの順に回転。
def CamPoseXYZ(rx, ry, rz, pxyz):
    mX = RotX(rx)
    mY = RotY(ry)
    mZ = RotZ(rz)
    m = mZ @ mY @ mX
    m[0:3,3] = pxyz[0:3]
    print(f'CamPoseXYZ=\n{m}')
    return m

def Proj(fovX, fovY, zNear, zFar):
    m = np.eye(4)
    w = 1.0/math.tan(fovX/2)
    h = 1.0/math.tan(fovX/2)
    q = zFar / (zFar - zNear)
    m[0,0] = w
    m[1,1] = h
    m[2,2] = q
    m[2,3] = 1
    m[3,2] = -q * zNear
    m[3,3] = 0
    return m

# Z-向きのカメラのメッシュ vertex listとtriangle list。
def Generate_CameraMeshZM(scale=0.1):
    sz = scale*1.5
    sx = scale
    sy = scale * 9.0 / 16.0
    p = np.zeros((5,3))
    p[0] = np.array(( 0, 0, 0))
    p[1] = np.array(( sx, sy,-sz))
    p[2] = np.array(( sx,-sy,-sz))
    p[3] = np.array((-sx,-sy,-sz))
    p[4] = np.array((-sx, sy,-sz))

    #print(p)

    t = np.zeros((6,3), dtype=np.int32)
    t[0] = np.array((2,1,0), dtype=np.int32)
    t[1] = np.array((3,2,0), dtype=np.int32)
    t[2] = np.array((4,3,0), dtype=np.int32)
    t[3] = np.array((1,4,0), dtype=np.int32)
    t[4] = np.array((2,3,1), dtype=np.int32)
    t[5] = np.array((4,1,3), dtype=np.int32)

    #print(t)
    return p, t

# Z+向きのカメラのメッシュ vertex listとtriangle list。
def Generate_CameraMeshZP(scale=0.1):
    p,t = Generate_CameraMeshZM(scale)
    p = Transform_PointList(p, RotY(math.pi))
    return p,t


# メッシュをPLYで保存。
def ExportMesh_Ply(p, v, fileName):
    ps=p.shape
    vs=v.shape

    with open(fileName, 'w', newline='\n') as f:
        f.write('ply\n')
        f.write('format ascii 1.0\n')

        f.write(f'element vertex {ps[0]}\n')
        f.write('property float x\n')
        f.write('property float y\n')
        f.write('property float z\n')

        f.write(f'element face {vs[0]}\n')
        f.write('property list uchar uint vertex_indices\n')
        f.write('end_header\n')

        for i in range(ps[0]):
            f.write(f'{p[i,0]} {p[i,1]} {p[i,2]}\n')

        for i in range(vs[0]):
            f.write(f'3 {int(v[i,0])} {int(v[i,1])} {int(v[i,2])}\n')

# 3次元点群をPLYで保存。
def ExportPoints_Ply(p, fileName):
    ps=p.shape

    with open(fileName, 'w', newline='\n') as f:
        f.write('ply\n')
        f.write('format ascii 1.0\n')

        f.write(f'element vertex {ps[0]}\n')
        f.write('property float x\n')
        f.write('property float y\n')
        f.write('property float z\n')
        f.write('end_header\n')

        for i in range(ps[0]):
            f.write(f'{p[i,0]} {p[i,1]} {p[i,2]}\n')

# 4x4行列Mに3次元座標p各点の縦ベクトルを右から掛ける。
# 変換後の3次元座標が戻ります。
def Transform_PointList(p, M):
    r = np.zeros(p.shape)
    for i in range(p.shape[0]):
        v = np.vstack([p[i, 0], p[i, 1], p[i, 2], 1])
        v = M @ v
        v /= v[3,0]
        r[i, 0:3] = np.hstack(v)[0:3]

    return r

# 4x4行列Mに3次元座標p各点の縦ベクトルを右から掛ける。
# プロジェクション変換後の2次元座標が戻ります。
def Project_PointList(p, M):
    ps = p.shape
    r = np.zeros((ps[0], 2))

    for i in range(p.shape[0]):
        v = np.vstack([p[i, 0], p[i, 1], p[i, 2], 1])
        v = M @ v
        v /= v[3,0]
        r[i, 0:2] = np.hstack(v)[0:2]

    return r


def MergeMesh(p0, v0, p1, v1):
    p0s=p0.shape
    p1s=p1.shape

    v0s=v0.shape
    v1s=v1.shape

    # v1の頂点番号をずらします。
    v1a = np.zeros(v1.shape, dtype=np.int32)
    for i in range(v1s[0]):
        v1a[i,:] = v1[i,:] + p0s[0]

    p=np.append(p0, p1).reshape(p0s[0]+p1s[0], p0s[1])
    v=np.append(v0, v1a).reshape(v0s[0]+v1s[0], v0s[1])

    return p,v

def Trans_Rot_to_TransformMat(t, R):
    t = t.flatten()

    #R = np.transpose(R)

    M = np.array([ \
        [  R[0,0],  R[0,1],  R[0,2], t[0]], \
        [  R[1,0],  R[1,1],  R[1,2], t[1]], \
        [  R[2,0],  R[2,1],  R[2,2], t[2]], \
        [  0,       0,       0,      1] ])

    print(f"R={R}")
    print(f"M={M}")
    return M

# 2つのカメラの関係図のPLYファイルを出力。
# カメラはOpenGL様式：Z-向き。
# カメラ姿勢M0, M1
def GeneratePLY_TwoCamPoseZM(M0, M1, fileName):
    p, v = Generate_CameraMeshZM()

    p0 = Transform_PointList(p, M0)
    p1 = Transform_PointList(p, M1)

    pW, vW = MergeMesh(p0, v, p1, v)
    ExportMesh_Ply(pW, vW, fileName)

# 2つのカメラの関係図のPLYファイルを出力。
# カメラはZ+向き。
# カメラ姿勢M0, M1 
def GeneratePLY_TwoCamPoseZP(M0, M1, fileName):
    p, v = Generate_CameraMeshZP()

    p0 = Transform_PointList(p, M0)
    p1 = Transform_PointList(p, M1)

    pW, vW = MergeMesh(p0, v, p1, v)
    ExportMesh_Ply(pW, vW, fileName)

def GeneratePLY_TwoCamTransRotZP(t,R,fileName):
    E4 = np.eye(4)
    M = Trans_Rot_to_TransformMat(t,R)

    GeneratePLY_TwoCamPoseZP(E4, M, fileName)

def BuildXi(x_list, y_list, f0):
    N = x_list.shape[0]
    assert N == y_list.shape[0]

    xi_list = []
    for i in range(N):
        x = x_list[i]
        y = y_list[i]

        xi=np.vstack([x*x, 2*x*y, y*y, 2*f0*x, 2*f0*y, f0*f0])
        xi_list.append(xi)
    return xi_list

# xi fundamental mat 
def BuildXi_F(x0_list, y0_list, x1_list, y1_list, f0):
    N = x0_list.shape[0]
    assert N == y0_list.shape[0]
    assert N == x1_list.shape[0]
    assert N == y1_list.shape[0]

    xi_list = []
    for i in range(N):
        x0 = x0_list[i]
        y0 = y0_list[i]
        x1 = x1_list[i]
        y1 = y1_list[i]

        xi=np.vstack([x0*x1, x0*y1, f0*x0, x1*y0, y0*y1, f0*y0, f0*x1, f0*y1, f0*f0])
        xi_list.append(xi)
    return xi_list

def BuildV0(x_list, y_list, f0):
    N = x_list.shape[0]
    assert N == y_list.shape[0]

    v0_list = []
    for i in range(N):
        x = x_list[i]
        y = y_list[i]

        v0=np.zeros((6,6))
        v0[0,0] = 4*x*x
        v0[0,1] = v0[1,0] = v0[1,2] = v0[2,1] = 4*x*y
        v0[1,1] = 4* (x*x+y*y)
        v0[2,2] = 4*y*y
        v0[0,3] = v0[3,0] = v0[1,4] = v0[4,1] = 4*f0*x
        v0[1,3] = v0[3,1] = v0[2,4] = v0[4,2] = 4*f0*y
        v0[3,3] = v0[4,4] = 4*f0*f0
        v0_list.append(v0)
    return v0_list

# V0 of Fundamental mat
def BuildV0_F(x0_list, y0_list, x1_list, y1_list, f0):
    N = x0_list.shape[0]
    assert N == x1_list.shape[0]
    assert N == y0_list.shape[0]
    assert N == y1_list.shape[0]

    v0_list = []
    for i in range(N):
        x0 = x0_list[i]
        x1 = x1_list[i]
        y0 = y0_list[i]
        y1 = y1_list[i]

        v0=np.zeros((9,9))
        v0[0,0] = x0*x0 + x1*x1
        v0[1,1] = x0*x0                 + y1*y1
        v0[3,3] =         x1*x1 + y0*y0
        v0[4,4] =                 y0*y0 + y1*y1
        v0[3,0] = v0[0,3] = v0[4,1] = v0[1,4] = x0*y0
        v0[0,1] = v0[1,0] = v0[3,4] = v0[4,3] = x1*y1
        v0[2,0] = v0[0,2] = v0[5,3] = v0[3,5] = f0*x1
        v0[2,1] = v0[1,2] = v0[5,4] = v0[4,5] = f0*y1
        v0[2,2] = v0[5,5] = v0[6,6] = v0[7,7] = f0*f0
        v0[6,0] = v0[0,6] = v0[7,1] = v0[1,7] = f0*x0
        v0[6,3] = v0[3,0] = f0*y0
        v0_list.append(v0)
    return v0_list

# θ→θ†
def ThetaDagger(t):
    t1 = t[0]
    t2 = t[1]
    t3 = t[2]
    t4 = t[3]
    t5 = t[4]
    t6 = t[5]
    t7 = t[6]
    t8 = t[7]
    t9 = t[8]
    thetaD = np.vstack([
        t5*t9 - t8*t6, 
        t6*t7 - t9*t4,
        t4*t8 - t7*t5,
        t8*t3 - t2*t9,
        t9*t1 - t3*t7,
 
        t7*t2 - t1*t8,
        t2*t6 - t5*t3,
        t3*t4 - t6*t1,
        t1*t5 - t4*t2 ])
    return thetaD

# θ→F
def ThetaToF(t):
    F = np.zeros((3,3))
    F[0,0] = t[0]
    F[0,1] = t[1]
    F[0,2] = t[2]

    F[1,0] = t[3]
    F[1,1] = t[4]
    F[1,2] = t[5]

    F[2,0] = t[6]
    F[2,1] = t[7]
    F[2,2] = t[8]
    return F

def Epipolar_Constraint_Error(x0_list, y0_list, x1_list, y1_list, f0, F):
    N = x0_list.shape[0]
    assert N == x1_list.shape[0]
    assert N == y0_list.shape[0]
    assert N == y1_list.shape[0]

    err_sum = 0.0

    for i in range(N):
        x0 = x0_list[i]
        y0 = y0_list[i]
        x1 = x1_list[i]
        y1 = y1_list[i]

        xy0 = np.vstack([
            x0/f0,
            y0/f0,
            1 ])
        
        xy1 = np.vstack([
            x1/f0,
            y1/f0,
            1 ])

        Fxy1 = np.matmul(F, xy1)

        #print(f"xy0={xy0}")
        #print(f"Fxy1={Fxy1}")

        err = np.vdot(xy0, Fxy1)
        err_sum += err
    return err_sum / N

#                       N
# 対称行列 M = (1/N) * Σ xi_i * xi_i^T
#                      i=0
# 最小二乗法。
def BuildM_LS(xi_list):
    N = len(xi_list)

    C = xi_list[0].shape[0]

    M = np.zeros((C, C))
    for i in range(N):
        xi = xi_list[i]

        a =  (1.0/N) * xi @ xi.T
        M += a
    return M


#                       N
# 対称行列 M = (1/N) * Σw_i * xi_i * xi_i^T
#                      i=0
# FNS法。
def BuildM(xi_list, w_list):
    N = len(xi_list)
    assert N == w_list.shape[0]

    C = xi_list[0].shape[0]

    M = np.zeros((C, C))
    for i in range(N):
        xi = xi_list[i]
        w  = w_list[i]

        a =  (w/N) * xi @ xi.T
        M += a
    return M

# 対称行列L
# FNS法。
def BuildL(xi_list, w_list, v0_list, ev):
    N = len(xi_list)
    assert N == w_list.shape[0]
    assert N == len(v0_list)

    C = xi_list[0].shape[0]

    L = np.zeros((C, C))
    for i in range(N):
        xi = xi_list[i]
        w  = w_list[i]
        v0 = v0_list[i]
        xi_dot_ev = (xi.T @ ev).item()

        a = (w*w/N) * xi_dot_ev * xi_dot_ev * v0
        L += a
    return L

def ReadTwoCamPoints(path):
    print(f"ReadTwoCamPoints({path})")
    x0_list=[]
    y0_list=[]
    x1_list=[]
    y1_list=[]
    with open(path) as f:
        r = csv.reader(f, quoting=csv.QUOTE_NONNUMERIC, delimiter=',')
        for l in r:
            x0_list.append(l[0])
            y0_list.append(l[1])
            x1_list.append(l[2])
            y1_list.append(l[3])
    return np.asarray(x0_list), np.asarray(y0_list), np.asarray(x1_list), np.asarray(y1_list)

def ReadPointXY3(path):
    x_list=[]
    y_list=[]
    with open(path) as f:
        r = csv.reader(f, quoting=csv.QUOTE_NONNUMERIC, delimiter=',')
        for l in r:
            x_list.append(l[0])
            y_list.append(l[1])
    return np.asarray(x_list), np.asarray(y_list)

# フィットした楕円を図示。
def Plot(ev, w_list, f0, x_list, y_list):
    # 楕円方程式の係数。
    # Ax^2 + 2Bxy + Cy^2 + 2f0(Dx + Ey) + f0^2*F = 0

    ev = ev.reshape(6)
    A=ev[0]
    B=ev[1]
    C=ev[2]
    D=ev[3]
    E=ev[4]
    F=ev[5]

    print(f"{A}x^2 + {2*B}xy + {C}y^2 + {2*f0*D}x + {2*f0*E}y + {f0*f0*F} = 0");

    # テスト
    #A=1
    #B=0
    #C=1
    #D=0
    #E=0
    #F=-10
    #x_min=-10
    #x_max=10

    x_min=np.min(x_list)
    x_max=np.max(x_list)
    y_min=np.min(y_list)
    y_max=np.max(y_list)

    # 楕円をプロットするために楕円上の点を集める。
    ex_list=[]
    ey_list=[]

    for x in np.arange(x_min-10, x_max+10, (x_max-x_min)/100):
        det = ((2*E*f0 + 2*B*x) ** 2) - 4*C*(F*f0*f0 + 2*D*f0*x + A*x*x)
        if 0 <= det:
            yP=(-2*E*f0-2*B*x + math.sqrt(det) ) / (2*C)
            yM=(-2*E*f0-2*B*x - math.sqrt(det) ) / (2*C)
            ex_list.append(x)
            ey_list.append(yP)
            ex_list.append(x)
            ey_list.append(yM)

    for y in np.arange(y_min-10, y_max+10, (y_max-y_min)/100):
        det = ((2*D*f0 + 2*B*y) ** 2) - 4*A*(F*f0*f0 + 2*E*f0*y + C*y*y)
        if 0 <= det:
            xP=(-2*D*f0-2*B*y + math.sqrt(det) ) / (2*A)
            xM=(-2*D*f0-2*B*y - math.sqrt(det) ) / (2*A)
            ex_list.append(xP)
            ey_list.append(y)
            ex_list.append(xM)
            ey_list.append(y)

    plt.scatter(ex_list,ey_list, s=5, marker='x', c='black')
    plt.scatter(x_list, y_list, s=1, c=w_list, marker='.', cmap='bwr')        
    plt.axis('equal')
    plt.colorbar()
    plt.title("Ransac FNS")
    plt.show()

