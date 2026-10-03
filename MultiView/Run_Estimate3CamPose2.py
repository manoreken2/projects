"""
3枚の画像から3カメラ間の相対姿勢 (t, R) と3D点群を推定します。(v2)

元版 Run_Estimate3CamPose.py は3カメラ共通の特徴点トリプレットのみから
全姿勢を推定するため、トリプレットが少ないと姿勢推定が破綻します。

v2 はトリプレットが少ない場合にrobustな別方針を採ります。
1. カメラペア(1,2)・(2,3)それぞれに2ビュー法
   (Run_FeatureMatch -> Ransac_TwoCam -> Fundamental_to_Trans_Rot) を適用し、
   相対姿勢 (t, R) を求める。t は長さ1に正規化されるため、
   ペア(2,3)のカメラ間距離はこの時点では不定。
2. ゲージ固定: カメラ1を原点・単位姿勢、距離(cam1,cam2)=1 とすると、
   カメラ3の向き R13 = R12*R23 は確定し、未知は距離比
   r = d23/d12 のスカラー1個のみ残る。
3. r を2とおりのゲージでトリプレットから推定し、良い方を選ぶ:
   - ゲージA: カメラ1を原点・単位姿勢、d12=1 に固定。
     トリプレット点をカメラ1・2で三角測量し、カメラ3の観測から
     共線条件 X - t12 = r*u + lambda*d を点ごと線形最小二乗で解き、
     重み付き中央値で r を推定。
   - ゲージB: カメラ2を原点・単位姿勢、d23=1 に固定。
     トリプレット点をカメラ2・3で三角測量し、カメラ1の観測から
     同じく共線条件で s = d12/d23 を推定し、r = 1/s を得る。
   両ゲージは同じ1パラメータ族(同じ R12,R13、r のみ異なる)を
   表すので、共通コスト(3カメラ全部で三角測量し直し、3カメラ全部への
   再投影画素距離のトリム済み平均)で2候補を比較して良い方を選ぶ。
   三角測量ベースラインや視線角の条件付けがゲージで異なるため、
   片方が退化していてももう片方で救えることがある。
   また 2ビュー姿勢分解の平行移動の向き判定はほぼ平面シーンで
   誤ることがあるため、t12/t23 の符号候補も全て試し、
   トリプレット共通コストで最良の組合せを選ぶ。
4. 選択後、共通コストを最小化する r の1次元探索で精製する。
   2ビュー法のRANSACは非決定で、ほぼ平面のシーンでは基礎行列Fが
   退化した別解に収束して姿勢分解も誤ることがある。その対策として、
   共通コストが悪い場合はペア姿勢推定からやり直し(--max_attempts)、
   リトライごとにclose_points_ratioを下げて正しいFのゲート通過を
   促す。最終コストが閾値(--max_reproj_cost)を超えれば失敗とする。

実行例:
    python Run_Estimate3CamPose2.py --img1 Synthetic_OctPrism/0001.png --img2 Synthetic_OctPrism/0002.png --img3 Synthetic_OctPrism/0003.png --focal_length 2667

注意:
- 平行移動は距離(cam1,cam2)=1 に正規化されたスケール(元版と同じゲージ)。
- 焦点距離が既知なら --focal_length を指定すること。
  未指定時は基礎行列Fから焦点距離を推定するが、狭FOV・短ベースラインでは
  虚焦点問題で失敗することがある(2ビュー法の数学的限界)。
"""

import argparse
import os

import numpy as np
from scipy.optimize import minimize_scalar

from Common import (
    CSV_Read_FeaturePointList,
    CSV_Read_TwoCam_MatchedPointList,
    CSV_Write_CamPose_list,
    CSV_Write_Point3d_list,
    Point2dPair,
    SelectF0_FromFeatureSpread,
    ThetaToF,
    Triangulation,
    weighted_median,
)
from Fundamental_to_CamParams import (
    Fundamental_to_FocalLength,
    Fundamental_to_Trans_Rot,
)
from PLYUtils import PLY_Export_MultiCam, PLY_Export_PointList
from Ransac_TwoCam import Ransac_TwoCam
from RegressorTwoCamFNS import RegressorTwoCamFNS
from RegressorTwoCamLSQ import RegressorTwoCamLSQ
from Run_FeatureMatch import Run_FeatureMatch
from Run_FeatureMatch3 import Run_FeatureMatch3


def Estimate_TwoCam_Pose(
    matched_points2d_csv,
    f0,
    focal_length=None,
    regressor="fns",
    ite_count=1000,
    loss_threshold=5.0,
    close_points_ratio=0.8,
):
    """
    2カメラ対応点CSVから相対姿勢 (t, R) を推定する。

    戻り値: 成功時 dict(t=カメラ2中心(カメラ1座標系,3x1,|t|=1), R=カメラ2向き,
            fl0=カメラ1焦点距離, fl1=カメラ2焦点距離, pp, valid_bitmap)
            失敗時 None
    """
    pp = CSV_Read_TwoCam_MatchedPointList(matched_points2d_csv)
    N = pp.get_point_count()
    if N < 8:
        print(f"Estimate_TwoCam_Pose: too few matched points ({N} < 8)")
        return None

    if regressor == "fns":
        model = RegressorTwoCamFNS(f0=f0)
    else:
        model = RegressorTwoCamLSQ(f0)

    ran = Ransac_TwoCam(
        f0=f0,
        close_points_ratio=close_points_ratio,
        ite_count=ite_count,
        loss_threshold=loss_threshold,
        model=model,
    )
    if ran.fit(pp) is None:
        print(f"Estimate_TwoCam_Pose: RANSAC failed. {matched_points2d_csv}")
        return None

    F = ThetaToF(ran.get_theta())
    valid_bitmap = ran.get_valid_bitmap()

    if focal_length is not None and focal_length > 0:
        fl0 = fl1 = float(focal_length)
    else:
        try:
            fl0, fl1 = Fundamental_to_FocalLength(F, f0)
        except ValueError as e:
            print(f"Estimate_TwoCam_Pose: Fundamental_to_FocalLength failed: {e}")
            print("  焦点距離を --focal_length で既知として指定すること。")
            return None
    print(f"Estimate_TwoCam_Pose: focal lengths = {fl0}, {fl1}")

    t, R = Fundamental_to_Trans_Rot(F, fl0, fl1, f0, pp, valid_bitmap)
    t = np.vstack(np.asarray(t).flatten())
    R = np.asarray(R)

    inlier_count = sum(1 for b in valid_bitmap if b)
    print(
        f"Estimate_TwoCam_Pose: {matched_points2d_csv} inliers={inlier_count}/{N}\n"
        f"trans={t.flatten()}\nrot=\n{R}"
    )
    return {"t": t, "R": R, "fl0": fl0, "fl1": fl1, "pp": pp, "valid_bitmap": valid_bitmap}


def Make_P(fl, R, C, f0):
    """
    姿勢 (C=カメラ中心, R=カメラ->世界回転) と焦点距離 fl から
    射影行列 P = diag(fl, fl, f0) @ [R^T | -R^T C] を作る。
    画像座標は画像中心原点の画素座標。
    """
    Rt = np.asarray(R).T
    C = np.asarray(C).reshape(3, 1)
    K = np.array([[fl, 0, 0], [0, fl, 0], [0, 0, f0]])
    return K @ np.concatenate((Rt, -Rt @ C), axis=1)


def Estimate_ratio_from_Triplets(
    fp_list,
    tri_idx,
    dep_idx,
    P_tri_a,
    P_tri_b,
    R_dep,
    C_dep0,
    w,
    fl_dep,
    f0,
    sin_angle_min=0.05,
    tag="",
):
    """
    3カメラ共通特徴点トリプレットから距離比 s を推定する(ゲージ汎用)。

    三角測量カメラ tri_idx の姿勢は完全に既知。
    従属カメラ dep_idx の中心は C_dep(s) = C_dep0 + s*w (w は単位ベクトル)
    と距離比 s に依存するので、三角測量点 X に対し共線条件
        X - C_dep0 = s*w + lambda*d   (d = R_dep * [x/fl, y/fl, 1])
    を点ごと線形最小二乗で解き、視線角の sin を重みとした中央値で統合する。

    戻り値: 成功時 dict(s, s_list, weight_list) 失敗時 None
    """
    nPoints = len(fp_list)
    if nPoints < 3:
        print(f"Estimate_ratio_from_Triplets{tag}: too few triplets ({nPoints} < 3)")
        return None

    # ---- 1. 三角測量カメラ2台でトリプレット点を三角測量 ----
    xy_a = np.array([fp.FeaturePointCoordOfCam(tri_idx[0]) for fp in fp_list])
    xy_b = np.array([fp.FeaturePointCoordOfCam(tri_idx[1]) for fp in fp_list])
    xy_dep = np.array([fp.FeaturePointCoordOfCam(dep_idx) for fp in fp_list])

    pp = Point2dPair(xy_a, xy_b)
    valid_bitmap = [True] * nPoints
    xyz_list, valid_bitmap, valid_point_count = Triangulation(
        pp, valid_bitmap, f0, P_tri_a, P_tri_b
    )
    print(
        f"Estimate_ratio_from_Triplets{tag}: triangulation "
        f"valid={valid_point_count}/{nPoints}"
    )

    # ---- 2. 点ごとの共線条件から s を解く ----
    w = np.asarray(w).flatten()
    C_dep0 = np.asarray(C_dep0).flatten()

    s_list = []
    weight_list = []
    for a in range(nPoints):
        if not valid_bitmap[a]:
            continue
        X = xyz_list[a, :]

        # 従属カメラの視線方向(世界座標系)。R_dep は既知なので s に依存しない。
        x_d, y_d = xy_dep[a]
        d = R_dep @ np.array([x_d / fl_dep, y_d / fl_dep, 1.0])

        # [w d] @ [s, lambda]^T = X - C_dep0 (3方程式, 2未知) を最小二乗で解く。
        A = np.concatenate([w.reshape(3, 1), d.reshape(3, 1)], axis=1)
        b = X - C_dep0
        sol = np.linalg.lstsq(A, b, rcond=None)[0].flatten()
        s_a, lam = float(sol[0]), float(sol[1])

        if s_a <= 0 or lam <= 0:
            # 距離が負 / 点が従属カメラの後方 -> 除外
            continue

        sin_th = np.linalg.norm(np.cross(w, d)) / np.linalg.norm(d)
        if sin_th < sin_angle_min:
            # 視線方向がベースライン方向にほぼ平行: 拘束が退化 -> 除外
            continue

        s_list.append(s_a)
        weight_list.append(sin_th)

    if len(s_list) < 3:
        print(
            f"Estimate_ratio_from_Triplets{tag}: "
            f"too few valid s estimates ({len(s_list)} < 3)"
        )
        return None

    s = float(weighted_median(s_list, weight_list))
    mad = float(np.median(np.abs(np.asarray(s_list) - s)))
    print(
        f"Estimate_ratio_from_Triplets{tag}: s={s} "
        f"(weighted median of {len(s_list)} points, MAD={mad}, "
        f"spread=[{min(s_list)}, {max(s_list)}])"
    )
    return {"s": s, "s_list": s_list, "weight_list": weight_list}


def Triangulate_Point_3cam(xy_list, Ps, f0):
    """
    3台の射影行列 Ps(3行目が f0*zc になる規約) と画素座標 xy_list=[(x,y),(x,y),(x,y)] から
    DLT(線形最小二乗)で3D点 X(3,) を求める。失敗時 None。

    射影条件 x*zc = P[0,:]X / fl、P[2,:]X = f0*zc より
    行方程式は x*P[2,:] - f0*P[0,:] = 0 。
    """
    rows = []
    for (x, y), P in zip(xy_list, Ps):
        rows.append(x * P[2, :] - f0 * P[0, :])
        rows.append(y * P[2, :] - f0 * P[1, :])
    A = np.asarray(rows)
    _, _, Vt = np.linalg.svd(A)
    Xh = Vt[-1, :]
    if abs(Xh[3]) < 1e-12:
        return None
    return Xh[:3] / Xh[3]


def Eval_config_reproj(r, xy1, xy2, xy3, t12, R12, u, R13, fl1, fl2, fl3, f0):
    """
    距離比 r = d23/d12 (ゲージ: カメラ1=原点・単位姿勢, |t12|=1) での
    3カメラ構成を作り、全トリプレット点を3カメラで三角測量し直して、
    3カメラ全部への再投影画素距離のトリム済み平均を返す。

    戻り値: (トリム済み平均再投影誤差[px], 使用点数, 三角測量点リスト)
    全点失敗時は (inf, 0, [])。
    """
    I3 = np.eye(3)
    C1 = np.zeros(3)
    C2 = t12.flatten()
    C3 = C2 + r * u

    Ps = [
        Make_P(fl1, I3, C1, f0),
        Make_P(fl2, R12, C2, f0),
        Make_P(fl3, R13, C3, f0),
    ]
    Rs = [I3, R12, R13]
    Cs = [C1, C2, C3]
    fls = [fl1, fl2, fl3]
    xys = [xy1, xy2, xy3]

    errs = []
    X_list = []
    nPoints = len(xy1)
    for a in range(nPoints):
        obs = [(xy1[a, 0], xy1[a, 1]), (xy2[a, 0], xy2[a, 1]), (xy3[a, 0], xy3[a, 1])]
        X = Triangulate_Point_3cam(obs, Ps, f0)
        if X is None:
            continue
        # 全カメラの前方にある点のみ(cheirality)。
        if not all((Rs[k].T @ (X - Cs[k]))[2] > 0 for k in range(3)):
            continue
        e = 0.0
        for k in range(3):
            Xc = Rs[k].T @ (X - Cs[k])
            px = fls[k] * Xc[0] / Xc[2]
            py = fls[k] * Xc[1] / Xc[2]
            e += np.hypot(px - obs[k][0], py - obs[k][1])
        errs.append(e / 3.0)
        X_list.append(X.reshape(3, 1))

    if not errs:
        return np.inf, 0, []
    errs = np.sort(np.asarray(errs))
    keep = max(1, int(np.ceil(errs.size * 0.8)))  # 上位20%をトリム(外れ値にrobust)
    return float(errs[:keep].mean()), errs.size, X_list


def Refine_r_by_total_reproj(r0, cost_fn, search_ratio=0.5):
    """
    r0 のまわりで共通コスト(3カメラ合計再投影誤差)を最小化する
    1次元探索で r を精製する。戻り値: (r_refined, cost)
    """
    lo = max(1e-6, r0 * (1.0 - search_ratio))
    hi = r0 * (1.0 + search_ratio)
    res = minimize_scalar(cost_fn, bounds=(lo, hi), method="bounded")
    print(
        f"Refine_r_by_total_reproj: r {r0} -> {res.x} "
        f"(cost {cost_fn(r0)} -> {res.fun} px)"
    )
    return float(res.x), float(res.fun)


def Estimate_distance_ratio(
    fp_list,
    result_pair12_points2d_csv,
    result_pair23_points2d_csv,
    f0,
    focal_length,
    regressor,
    ite_count,
    loss_threshold,
    close_points_ratio,
    sin_angle_min,
):
    """
    ペア2ビュー姿勢推定 + 2ゲージ距離比推定 + 共通コスト選択を1回行う。

    戻り値: 成功時 dict(cost=共通コスト[px], r=距離比d23/d12, gauge='A'/'B',
            t12, R12, t23, R23, fl_cam1, fl_cam2, fl_cam3, R13, u)
            失敗時 None
    """
    # ---- ペアごとの2ビュー姿勢推定 ----
    pair12 = Estimate_TwoCam_Pose(
        result_pair12_points2d_csv,
        f0,
        focal_length,
        regressor,
        ite_count,
        loss_threshold,
        close_points_ratio,
    )
    if pair12 is None:
        return None

    pair23 = Estimate_TwoCam_Pose(
        result_pair23_points2d_csv,
        f0,
        focal_length,
        regressor,
        ite_count,
        loss_threshold,
        close_points_ratio,
    )
    if pair23 is None:
        return None

    t12, R12 = pair12["t"], pair12["R"]
    t23, R23 = pair23["t"], pair23["R"]
    # カメラ2の焦点距離はペア推定で2つの候補が出る。平均を使う。
    fl_cam1 = pair12["fl0"]
    fl_cam2 = 0.5 * (pair12["fl1"] + pair23["fl0"])
    fl_cam3 = pair23["fl1"]

    I3 = np.eye(3)

    # ---- 距離比 r = d23/d12 を2ゲージで推定 ----
    xy1 = np.array([fp.FeaturePointCoordOfCam(0) for fp in fp_list])
    xy2 = np.array([fp.FeaturePointCoordOfCam(1) for fp in fp_list])
    xy3 = np.array([fp.FeaturePointCoordOfCam(2) for fp in fp_list])

    # 平行移動 t12, t23 の「向き」は、2ビュー姿勢分解の4候補判定
    # (両カメラ前方の点数)がほぼ平面のシーンで誤ることがある
    # (回転は正しく t だけ逆向きになる実測例あり)。
    # トリプレット(3ビュー)データは判別力があるので、t の符号候補を
    # 全て試してトリプレット共通コストで最良を選ぶ。
    best = None
    for s12 in (1.0, -1.0):
        for s23 in (1.0, -1.0):
            t12c = s12 * t12
            t23c = s23 * t23
            R13c = R12 @ R23
            uc = (R12 @ t23c).flatten()  # カメラ2->3 の向き(世界=カメラ1座標系)

            # ゲージA: カメラ1=原点・単位姿勢、d12=1。カメラ1・2で三角測量、
            # 従属カメラ=カメラ3、C3(r) = t12 + r*u。
            estA = Estimate_ratio_from_Triplets(
                fp_list,
                (0, 1),
                2,
                Make_P(fl_cam1, I3, np.zeros(3), f0),
                Make_P(fl_cam2, R12, t12c, f0),
                R13c,
                t12c.flatten(),
                uc,
                fl_cam3,
                f0,
                sin_angle_min,
                tag=f" [s12={s12}, s23={s23}, gauge A: cam1 fixed, dependent=cam3]",
            )
            rA = estA["s"] if estA is not None else None

            # ゲージB: カメラ2=原点・単位姿勢、d23=1(世界=カメラ2座標系)。
            # カメラ2・3で三角測量、従属カメラ=カメラ1、C1(s) = s*w
            # (w = -R12^T*t12: カメラ2からカメラ1を見る単位方向)。
            # 得られる s = d12/d23 なので r = 1/s。
            wB = -(R12.T @ t12c).flatten()
            estB = Estimate_ratio_from_Triplets(
                fp_list,
                (1, 2),
                0,
                Make_P(fl_cam2, I3, np.zeros(3), f0),
                Make_P(fl_cam3, R23, t23c, f0),
                R12.T,
                np.zeros(3),
                wB,
                fl_cam1,
                f0,
                sin_angle_min,
                tag=f" [s12={s12}, s23={s23}, gauge B: cam2 fixed, dependent=cam1]",
            )
            rB = 1.0 / estB["s"] if estB is not None else None

            candidates = []
            if rA is not None:
                candidates.append(("A", rA))
            if rB is not None:
                candidates.append(("B", rB))

            for name, r in candidates:
                c = Eval_config_reproj(
                    r, xy1, xy2, xy3, t12c, R12, uc, R13c,
                    fl_cam1, fl_cam2, fl_cam3, f0,
                )[0]
                print(f"combo s12={s12} s23={s23} gauge {name}: r={r}, cost={c}px")
                if best is None or c < best["cost"]:
                    best = {
                        "cost": c,
                        "r": r,
                        "gauge": name,
                        "s12": s12,
                        "s23": s23,
                        "t12": t12c,
                        "R12": R12,
                        "t23": t23c,
                        "R23": R23,
                        "fl_cam1": fl_cam1,
                        "fl_cam2": fl_cam2,
                        "fl_cam3": fl_cam3,
                        "R13": R13c,
                        "u": uc,
                    }

    if best is None:
        return None

    print(
        f"selected combo s12={best['s12']} s23={best['s23']} "
        f"gauge {best['gauge']} (cost={best['cost']}px)"
    )
    return best


def Run_Estimate3CamPose2(
    img1_path,
    cam1_id,
    img2_path,
    cam2_id,
    img3_path,
    cam3_id,
    result_pair12_points2d_csv,
    result_pair23_points2d_csv,
    result_feature_points_list_csv,
    result_cam_pose_csv,
    result_cam_pose_ply,
    result_points3d_csv,
    result_points3d_ply,
    Lowes_ratio,
    homography_ransac_threshold,
    f0=-1.0,
    focal_length=None,
    regressor="fns",
    ite_count=1000,
    loss_threshold=5.0,
    close_points_ratio=0.8,
    sin_angle_min=0.05,
    refine_s=True,
    max_attempts=3,
    max_reproj_cost=50.0,
):
    """
    f0: f0 parameter. if it is negative number, f0 is calculated from feature point spread.
    focal_length: 既知の焦点距離 fx=fy (px)。None なら F から推定(失敗することがある)。
    max_attempts: 距離比推定の共通再投影コストが悪い場合、2ビュー姿勢推定
                  (RANSACは非決定)をやり直す最大回数。
    max_reproj_cost: 最終的な共通再投影コストがこの値[px]を超えていれば失敗とする。
    """
    # ---- 1. ペア(1,2)・(2,3)の対応点CSV ----
    base12, _ = os.path.splitext(result_pair12_points2d_csv)
    b = Run_FeatureMatch(
        img1_path,
        img2_path,
        result_pair12_points2d_csv,
        Lowes_ratio,
        homography_ransac_threshold,
        base12 + ".png",
    )
    if b is not True:
        print("Run_Estimate3CamPose2: Run_FeatureMatch(1,2) failed.")
        return False

    base23, _ = os.path.splitext(result_pair23_points2d_csv)
    b = Run_FeatureMatch(
        img2_path,
        img3_path,
        result_pair23_points2d_csv,
        Lowes_ratio,
        homography_ransac_threshold,
        base23 + ".png",
    )
    if b is not True:
        print("Run_Estimate3CamPose2: Run_FeatureMatch(2,3) failed.")
        return False

    # ---- 2. 3カメラ共通特徴点トリプレットCSV ----
    b = Run_FeatureMatch3(
        img1_path,
        cam1_id,
        img2_path,
        cam2_id,
        img3_path,
        cam3_id,
        result_feature_points_list_csv,
        Lowes_ratio,
        homography_ransac_threshold,
    )
    if b is not True:
        print("Run_Estimate3CamPose2: Run_FeatureMatch3 failed.")
        return False

    # ---- 3. f0 決定(元版と同じ規約。全工程で共通の f0 を使う) ----
    if f0 < 0:
        f0_auto = SelectF0_FromFeatureSpread(result_feature_points_list_csv)
        if f0_auto is None:
            raise RuntimeError("auto f0: failed to compute!")
        print(f"auto f0 = {f0_auto}")
        f0 = f0_auto

    # ---- 4. 2ビュー姿勢推定 + 2ゲージ距離比推定 ----
    # 2ビュー法のRANSACは非決定で、ほぼ平面のシーンでは基礎行列Fが
    # 退化した別解に収束することがある。その場合は姿勢分解も誤るため、
    # 最終構成の共通再投影コストを判定基準に、ペア姿勢推定からやり直す。
    fp_list = CSV_Read_FeaturePointList(result_feature_points_list_csv, f0)
    xy1 = np.array([fp.FeaturePointCoordOfCam(0) for fp in fp_list])
    xy2 = np.array([fp.FeaturePointCoordOfCam(1) for fp in fp_list])
    xy3 = np.array([fp.FeaturePointCoordOfCam(2) for fp in fp_list])

    best = None
    for attempt in range(max(1, max_attempts)):
        # リトライごとに close_points_ratio を下げる: ほぼ平面のシーンでは
        # 正しい基礎行列Fはインライアー数が少なく、既定のゲート(0.8)で
        # 弾かれて退化解が勝つことがある(ゲート通過モデルのうち最小lossで選択)。
        cpr = max(0.4, close_points_ratio * (0.8 ** attempt))
        if attempt > 0:
            print(
                f"Run_Estimate3CamPose2: retry {attempt} "
                f"(pair pose re-estimation, close_points_ratio={cpr})"
            )
        res = Estimate_distance_ratio(
            fp_list,
            result_pair12_points2d_csv,
            result_pair23_points2d_csv,
            f0,
            focal_length,
            regressor,
            ite_count,
            loss_threshold,
            cpr,
            sin_angle_min,
        )
        if res is None:
            continue

        r = res["r"]
        cost = res["cost"]

        def cost_fn(rr, res=res):
            return Eval_config_reproj(
                rr,
                xy1,
                xy2,
                xy3,
                res["t12"],
                res["R12"],
                res["u"],
                res["R13"],
                res["fl_cam1"],
                res["fl_cam2"],
                res["fl_cam3"],
                f0,
            )[0]

        if refine_s:
            r, cost = Refine_r_by_total_reproj(r, cost_fn)
        res["r"] = r
        res["cost"] = cost

        if best is None or cost < best["cost"]:
            best = res
        if best["cost"] < max_reproj_cost * 0.2:
            break

    if best is None:
        return False

    if best["cost"] > max_reproj_cost:
        print(
            f"Run_Estimate3CamPose2: FAILED. total reproj cost={best['cost']}px "
            f"> {max_reproj_cost}px. ペア姿勢推定が誤解(退化)の可能性。"
            " --max_attempts を増やす/対応点品質を上げる/焦点距離を既知指定すること。"
        )
        return False

    # ---- 5. 最終姿勢の合成(ゲージ: カメラ1=原点・単位姿勢, |t12|=1) ----
    t12, R12, R13, u, r = best["t12"], best["R12"], best["R13"], best["u"], best["r"]
    I3 = np.eye(3)
    tk_list = [
        np.zeros((3, 1)),
        t12,
        t12 + r * u.reshape(3, 1),
    ]
    Rk_list = [I3, R12, R13]

    cam_id_list = [cam1_id, cam2_id, cam3_id]
    CSV_Write_CamPose_list(result_cam_pose_csv, tk_list, Rk_list, cam_id_list)
    PLY_Export_MultiCam(result_cam_pose_ply, tk_list, Rk_list)

    # 3D点群: 確定した距離比 r で3カメラを使い三角測量し直して出力。
    _, n_used, X_list = Eval_config_reproj(
        r,
        xy1,
        xy2,
        xy3,
        t12,
        R12,
        u,
        R13,
        best["fl_cam1"],
        best["fl_cam2"],
        best["fl_cam3"],
        f0,
    )
    CSV_Write_Point3d_list(result_points3d_csv, X_list)
    PLY_Export_PointList(result_points3d_ply, X_list)

    print(
        f"Run_Estimate3CamPose2: OK. r=d23/d12={r} (gauge {best['gauge']}), "
        f"total reproj cost={best['cost']}px, 3d points={len(X_list)}/{n_used}"
    )
    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="estimate 3-cam pose from 3 images (2-view pairs + triplet scale estimation)"
    )
    parser.add_argument(
        "--img1",
        type=str,
        help="image 1 png file",
        default="Synthetic_OctPrism/0001.png",
    )
    parser.add_argument(
        "--img2",
        type=str,
        help="image 2 png file",
        default="Synthetic_OctPrism/0002.png",
    )
    parser.add_argument(
        "--img3",
        type=str,
        help="image 3 png file",
        default="Synthetic_OctPrism/0003.png",
    )
    parser.add_argument(
        "--cam1_id",
        type=int,
        default=1,
        help="image 1 camera id",
    )
    parser.add_argument(
        "--cam2_id",
        type=int,
        default=2,
        help="image 2 camera id",
    )
    parser.add_argument(
        "--cam3_id",
        type=int,
        default=3,
        help="image 3 camera id",
    )
    parser.add_argument(
        "--result_pair12_points2d_csv",
        type=str,
        help="CSV file to write cam1-cam2 matched point list",
        default="tmp/result_pair_points2d_0001_0002.csv",
    )
    parser.add_argument(
        "--result_pair23_points2d_csv",
        type=str,
        help="CSV file to write cam2-cam3 matched point list",
        default="tmp/result_pair_points2d_0002_0003.csv",
    )
    parser.add_argument(
        "--result_feature_points_list_csv",
        type=str,
        help="CSV file to write 3 cam feature point (x,y) list",
        default="tmp/result_feature_points2d_0001_0002_0003.csv",
    )
    parser.add_argument(
        "--result_cam_pose_csv",
        type=str,
        help="output camera pose CSV file",
        default="tmp/result_campose_0001_0002_0003.csv",
    )
    parser.add_argument(
        "--result_cam_pose_ply",
        type=str,
        help="output camera pose PLY file",
        default="tmp/result_campose_0001_0002_0003.ply",
    )
    parser.add_argument(
        "--result_points3d_csv",
        type=str,
        help="output 3d points CSV file",
        default="tmp/result_points3d_0001_0002_0003.csv",
    )
    parser.add_argument(
        "--result_points3d_ply",
        type=str,
        help="output 3d points PLY file",
        default="tmp/result_points3d_0001_0002_0003.ply",
    )
    parser.add_argument(
        "--lowes_ratio",
        type=float,
        default=0.7,
        help="Lowe ratio test threshold (0.7 default, larger = more candidates)",
    )
    parser.add_argument(
        "--ransac_threshold",
        type=float,
        default=35.0,
        help="RANSAC homography reprojection threshold (larger = more candidates)",
    )
    parser.add_argument(
        "--f0",
        type=float,
        help="f0 parameter in pixel. When negative, calculated from feature point spread.",
        default=-1.0,
    )
    parser.add_argument(
        "--focal_length",
        type=float,
        default=None,
        help="known camera focal length fx=fy (px). When set, skip focal-length "
        "estimation from F and fix K focal to this value.",
    )
    parser.add_argument(
        "--regressor",
        type=str,
        choices=["lsq", "fns"],
        default="fns",
        help="regressor to use for two-cam pose estimation (lsq or fns)",
    )
    parser.add_argument(
        "--ite_count", type=int, default=1000, help="RANSAC maximum iterations"
    )
    parser.add_argument(
        "--loss_threshold",
        type=float,
        default=5.0,
        help="サンプソン誤差J threshold",
    )
    parser.add_argument(
        "--close_points_ratio",
        type=float,
        default=0.8,
        help="RANSACでインライアーと判定されるべき点の数の比率",
    )
    parser.add_argument(
        "--sin_angle_min",
        type=float,
        default=0.05,
        help="min sin(angle) between dependent camera baseline direction and "
        "dependent camera ray direction. smaller angle = degenerate constraint, excluded.",
    )
    parser.add_argument(
        "--no_refine_s",
        action="store_false",
        dest="refine_s",
        default=True,
        help="disable 1D total-reprojection-error refinement of the distance ratio r "
        "(use weighted-median estimate only).",
    )
    parser.add_argument(
        "--max_attempts",
        type=int,
        default=3,
        help="max retries of two-cam pair pose estimation when the final total "
        "reprojection cost is bad (RANSAC is nondeterministic).",
    )
    parser.add_argument(
        "--max_reproj_cost",
        type=float,
        default=50.0,
        help="fail with error when final total reprojection cost exceeds this [px].",
    )
    args = parser.parse_args()

    b = Run_Estimate3CamPose2(
        args.img1,
        args.cam1_id,
        args.img2,
        args.cam2_id,
        args.img3,
        args.cam3_id,
        args.result_pair12_points2d_csv,
        args.result_pair23_points2d_csv,
        args.result_feature_points_list_csv,
        args.result_cam_pose_csv,
        args.result_cam_pose_ply,
        args.result_points3d_csv,
        args.result_points3d_ply,
        args.lowes_ratio,
        args.ransac_threshold,
        args.f0,
        args.focal_length,
        args.regressor,
        args.ite_count,
        args.loss_threshold,
        args.close_points_ratio,
        args.sin_angle_min,
        args.refine_s,
        args.max_attempts,
        args.max_reproj_cost,
    )
    if b is not True:
        raise SystemExit(1)
