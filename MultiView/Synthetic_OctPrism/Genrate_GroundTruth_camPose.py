import numpy as np

path = "campose_ground_truth.csv"
M = 24
r = 10.0

with open(path, "w", newline="\n") as f:
    f.write("camera_id, tX, tY, tZ, r00, r01, r02, r10, r11, r12, r20, r21, r22\n")

    for k in range(M):

        # ワールドにカメラを置く行列。
        # t: 1行3列 列ベクトル
        # R: 3行3列 回転ベクトル
        # ┌              ┐
        # │R00 R01 R02 tX│
        # │R10 R11 R12 tY│
        # │R20 R21 R22 tZ│
        # │ 0   0   0   1│
        # └              ┘

        t_theta = 2.0 * np.pi * k / M
        tct = np.cos(t_theta)
        tst = np.sin(t_theta)

        if 12 <= k:
            # 後半のカメラは1度進んでいる。
            r_theta = 2.0 * np.pi * (k / M + 1.0 / 360.0)
        else:
            r_theta = 2.0 * np.pi * k / M
        rct = np.cos(-r_theta)
        rst = np.sin(-r_theta)

        # 最初はz+にいて、x+方向に移動、原点の周りを回転する。
        # y-方向に0.005ずつ移動。
        t = np.array([[r * tst], [r * 0.0 - k * 0.005], [r * tct]])

        # y軸まわり-theta回転。
        R = np.array([[rct, 0, rst], [0, 1, 0], [-rst, 0, rct]])

        f.write(f"{k}, ")

        f.write(f"{t[0,0]}, {t[1,0]}, {t[2,0]}, ")

        f.write(f"{R[0,0]}, {R[0,1]}, {R[0,2]}, ")
        f.write(f"{R[1,0]}, {R[1,1]}, {R[1,2]}, ")
        f.write(f"{R[2,0]}, {R[2,1]}, {R[2,2]}\n")
