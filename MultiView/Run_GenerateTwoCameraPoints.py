import matplotlib.pyplot as plt
import csv
import numpy as np
from numpy.linalg import eigh
import math

def RotX(x):
    c=math.cos(x)
    s=math.sin(x)
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

def CamPoseXYZ(rx, ry, rz, pxyz):
    mX = RotX(rx)
    mY = RotY(ry)
    mZ = RotZ(rz)
    m = np.matmul(mZ, np.matmul(mY, mX))
    m[0:3,3] = pxyz[0:3]
    print(f'CamPoseXYZ={m}')
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

def PltImg(viewProj, C):
    x_list = []
    y_list = []

    # 中心(0,y,-z) 半径5

    z = -10.0
    ra =  5.0
    sy = 0.25

    N = 20

    for y in range(N):
        for x in range(N):
            t = math.pi * (0.5 + ((x-N/2) / (4*N)))
            dx = ra * math.cos(t)
            dy = sy * y
            dz = z + ra * math.sin(t)
            #print(f'dx={dx}, y={dy}, dz={dz}')
            xyz=np.array([dx, dy, dz, 1])

            r = np.matmul(viewProj, xyz)
            r = r / r[3]

            x_list.append(r[0])
            y_list.append(r[1])

    plt.scatter(x_list, y_list, s=1, marker=',', c=C)        
    return x_list, y_list


def main():
    # 右手座標系、縦行列。

    # 正方形画像。
    fovX = math.pi/2
    fovY = math.pi/2
    nearZ = 0.1
    farZ = 100.0
    proj = Proj(fovX, fovY, nearZ, farZ)
    print(f'proj={proj}')

    # Refカメラ。原点正面向き。
    camPoseRef = np.eye(4)
    print(f'cpRef={camPoseRef}')

    viewProjRef = np.matmul(proj, np.linalg.inv(camPoseRef))
    xr, yr = PltImg(viewProjRef, 'blue')

    # テストカメラ。pos=(1,0,0) yrot=pi/20
    viewProjTest = np.matmul(proj, np.linalg.inv(CamPoseXYZ(-math.pi/100, math.pi/20, math.pi/50, np.array([1, 0, 0]))))
    xt, yt = PltImg(viewProjTest, 'red')

    with open('twoCamPoints.csv', 'w') as f:
        for i in range(len(xr)):
            f.write(f'{xr[i]}, {yr[i]}, {xt[i]}, {yt[i]}\n')

    plt.axis('equal')
    plt.title("Camera Projected")
    plt.show()

    #camPose_Ref = np.eye(4)
    #print(camPose_Ref)

if __name__ == "__main__":
    main()

