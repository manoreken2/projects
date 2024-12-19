# 3章 手順3.8 RANSAC 2つの画像の対応点から基礎行列Fを求める。

from Common import *
from Rank_Correction import Rank_Correction
from Fundamental_to_CamParams import *
from Ransac_TwoCam import *
from LSQTwoCamRegressor import LSQTwoCamRegressor
from FNSTwoCamRegressor import FNSTwoCamRegressor
from mpl_toolkits import mplot3d
import numpy as np
import matplotlib.pyplot as plt

def PlotValidPoints(xy_list, c_list, loss_list):
    plt.scatter(xy_list[:,0], xy_list[:,1], s=3, c=c_list, marker='.', cmap='bwr')

    for i, l in enumerate(loss_list):
        v = float(l)
        plt.annotate(f"{i}:{v:.1f}", (xy_list[i,0], xy_list[i,1]), )

    plt.axis('equal')
    plt.colorbar()
    plt.title("Valid Points")
    plt.show()

def Plot3D(xyz_list, c_list):
    x_list = np.array(xyz_list)[:,0]
    y_list = np.array(xyz_list)[:,1]
    z_list = np.array(xyz_list)[:,2]

    fig = plt.figure()
 
    ax = plt.axes(projection ='3d')
 
    ax.scatter(x_list, y_list, z_list, c = c_list)
 
    ax.set_title('Estimated 3D points')
    plt.show()

def main():
    f0=1

    xy0_list, xy1_list = CSV_Read_TwoCamPointList('twoCamPoints410_outlier10.csv')
    N = xy0_list.shape[0]

    ran = Ransac_TwoCam(close_points=N//2, ite_count=300, model=FNSTwoCamRegressor())
    ran.fit(xy0_list, xy1_list)

    theta = ran.get_theta()
    F = ThetaToF(theta)

    fl0, fl1 = Fundamental_to_FocalLength(F, f0)
    print(f"Focal length = {fl0}, {fl1}")

    inlier_flag_list = ran.get_inlier_flag_list()
    inlier_core_flag_list = ran.get_inlier_core_flag_list()
    loss_list = ran.calc_loss(xy0_list, xy1_list)

    #PlotValidPoints(xy0_list, c_list, loss_list);

    err = Epipolar_Constraint_Error(xy0_list, xy1_list, inlier_flag_list, f0, F)
    print(f"Epipolar Constraint error = {err}")

    t, R = Fundamental_to_Trans_Rot(F, fl0, fl1, f0, xy0_list, xy1_list, inlier_flag_list)
    print(f"trans={t}\nrot={R}")

    PLY_Export_TwoCam(t, R, 'twoCamEst2r.ply')

    P0, P1 = TwoCamMat(f0, fl0, fl1, t, R)

    AdjustTwoPoints(xy0_list, xy1_list, inlier_flag_list, theta, f0)

    xyz_list = Triangulation(xy0_list, xy1_list, inlier_flag_list, f0, P0, P1)

    #Plot3D(xyz_list, new_loss_list)

    PLY_Export_PointList(InlierPointList(xyz_list, inlier_flag_list), "ResultPoints3D.ply")
    PLY_Export_PointList(InlierPointList(xyz_list, inlier_core_flag_list), "ResultCorePoints3D.ply")

if __name__ == "__main__":
    main()



   
