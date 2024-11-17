# 3章 手順3.8 RANSAC 2つの画像の対応点から基礎行列Fを求める。

from Common import *
from Rank_Correction import Rank_Correction
from Fundamental_to_CamParams import Fundamental_to_Trans_Rot, FundamentalToFocalLength
from Ransac_TwoCam import *
from LSQTwoCamRegressor import LSQTwoCamRegressor

def main():
    f0=1

    xy0_list, xy1_list = ReadTwoCamPoints('twoCamPoints2.csv')
    N=xy0_list.shape[0]
    ran = Ransac_TwoCam(d=N//2, model=LSQTwoCamRegressor())
    ran.fit(xy0_list, xy1_list)

    theta = ran.get_theta()
    c_list = ran.get_c_list()

    F = ThetaToF(theta)

    err = Epipolar_Constraint_Error(xy0_list, xy1_list, f0, F)
    print(f"Epipolar Constraint error = {err}")

    fl0, fl1 = FundamentalToFocalLength(F, 1.0)
    print(f"Focal length = {fl0} {fl1}")

    t, R = Fundamental_to_Trans_Rot(F, fl0, fl1, f0, xy0_list, xy1_list)

    print(f"trans={t}\nrot={R}")

    GeneratePLY_TwoCamTransRotZP(t, R, 'twoCamEst2r.ply')

if __name__ == "__main__":
    main()




