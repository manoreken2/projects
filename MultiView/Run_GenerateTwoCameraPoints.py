import matplotlib.pyplot as plt
import csv
import numpy as np
from numpy.linalg import eigh
import math
from Common import *
from numpy.random import default_rng

def GenPoints():
    # 中心(0, 0, 10) 半径5 円筒。

    z  = 10.0
    ra =  5.0
    sy =  0.25

    N = 20

    p = np.zeros((N*N, 3))

    for y in range(N):
        for x in range(N):
            t = math.pi * (0.5 + ((x-N/2) / (4*N)))
            px = ra * math.cos(t)
            py = sy * (y - N/2.0)
            pz = z + ra * math.sin(t)
            p[x + N*y] = np.array([px,py,pz])

    return p

def AppendOutliers2D(f, N, scale):
    p = np.zeros((N,3))
    for i in range(N):
        rng = default_rng()
        x0=rng.random()*scale
        y0=rng.random()*scale
        x1=rng.random()*scale
        y1=rng.random()*scale
        
        f.write(f'{x0}, {y0}, {x1}, {y1}\n')

def PltPointsXY(pointsXY, C):
    if pointsXY.shape[0]==0:
        return
    else:
        x_list = pointsXY[:,0]
        y_list = pointsXY[:,1]
        plt.scatter(x_list, y_list, s=1, marker=',', c=C)


def main():
    # 左手座標系、縦行列。

    # 点群生成。
    p = GenPoints()
    
    fovX = math.pi/2
    fovY = math.pi/2
    nearZ = 0.1
    farZ = 100.0
    proj = Proj(fovX, fovY, nearZ, farZ)
    print(f'proj=\n{proj}')

    # Refカメラ。原点 Z+向き。
    camPoseRef = np.eye(4)

    print(f'cpRef=\n{camPoseRef}')

    viewProjRef = proj @ np.linalg.inv(camPoseRef)

    # テストカメラ。pos=(-1,0,1) 大体Z+向き (平行の関係を避ける)
    # テストカメラのほうが被写体から遠い。
    camPoseTest = CamPoseXYZ(
            math.pi/100,
            math.pi/20,
            0,
            np.array([-1/math.sqrt(2.0), 0, 1/math.sqrt(2.0)]))
    viewProjTest = np.matmul(proj, np.linalg.inv(camPoseTest))

    xyR = Project_PointList(p, viewProjRef)

    PltPointsXY(xyR, 'blue')

    xyT = Project_PointList(p, viewProjTest)

    PltPointsXY(xyT, 'red')

    GeneratePLY_TwoCamPoseZP(camPoseRef, camPoseTest, 'twoCamPointsCameras2.ply')
    ExportPoints_Ply(p, 'twoCamPointsPointList2.ply')

    numPoints = xyR.shape[0]
    numPointsT=xyT.shape[0]
    assert(numPoints==numPointsT)

    with open('twoCamPoints2_outlier10.csv', 'w') as f:
        for i in range(numPoints):
            f.write(f'{xyR[i,0]}, {xyR[i,1]}, {xyT[i,0]}, {xyT[i,1]}\n')
        AppendOutliers2D(f, N=10, scale=10.0)

    plt.axis('equal')
    plt.title("Camera Projected. blue=ref")
    plt.show()

if __name__ == "__main__":
    main()

