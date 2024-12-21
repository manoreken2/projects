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

def PlotValidPoints(pp, loss_list):
    plt.scatter(pp.a[:,0], pp.a[:,1], s=3, c=loss_list, marker='.', cmap='bwr')

    #for i, l in enumerate(loss_list):
    #    v = float(l)
    #    plt.annotate(f"{i}:{v:.1f}", (xy_list[i,0], xy_list[i,1]), )

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

    pp = CSV_Read_TwoCamPointList('twoCamPoints410_outlier10.csv')
    N = pp.get_point_count()

    ran = Ransac_TwoCam(close_points=N//2, ite_count=30, model=FNSTwoCamRegressor())
    ran.fit(pp)

    theta = ran.get_theta()
    F = ThetaToF(theta)

    fl0, fl1 = Fundamental_to_FocalLength(F, f0)
    print(f"Focal length = {fl0}, {fl1}")

    valid_bitmap  = ran.get_valid_bitmap()
    picked_up_ids = ran.get_picked_up_ids()

    #PlotValidPoints(pp, valid_bitmap);

    err = Epipolar_Constraint_Error(pp, valid_bitmap, f0, F)
    print(f"Epipolar Constraint error = {err}")

    t, R = Fundamental_to_Trans_Rot(F, fl0, fl1, f0, pp, valid_bitmap)
    print(f"trans={t}\nrot={R}")

    PLY_Export_TwoCam(t, R, 'twoCamEst2r.ply')

    P0, P1 = TwoCamMat(f0, fl0, fl1, t, R)

    AdjustTwoPoints(pp, valid_bitmap, theta, f0)

    xyz_list, valid_bitmap = Triangulation(pp, valid_bitmap, f0, P0, P1)

    #Plot3D(xyz_list, new_loss_list)

    PLY_Export_PointList(InlierPointList_from_bitmap(xyz_list, valid_bitmap), "ResultPoints3D.ply")
    PLY_Export_PointList(InlierPointList_from_inlierIdList(xyz_list, picked_up_ids), "ResultCorePoints3D.ply")

if __name__ == "__main__":
    main()



   
