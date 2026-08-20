import argparse
import csv
import os
import statistics

import numpy as np

from Common import (
    CSV_Write_CamPose_list,
    PLY_Export_MultiCam,
    CSV_Write_Point3d_list,
    Read_CamPose_CSV,
    inv_rigid,
    SelectF0_FromFeatureSpread,
    weighted_median,
    average_rotation,
    apply_similarity_to_pose,
    Build_RangeCamPose_Files,
)

from Run_BundleAdjustment import Run_BundleAdjustment


def estimate_similarity_transform(final, d, overlap_cams, weights=None):
    """
    重複カメラの姿勢行列のペアから、フレーム間の相似変換 (s, R, t) を
    頑健に推定する。

    各ファイルは並進tに未知の絶対スケールを持つため、剛体変換ではなく
    スケールsを含む相似変換で接続する。

      - スケールs: 重複カメラのペアから得たベースライン長の比の重み付き中央値
                    s_ij = |final[cj] - final[ci]| / |d[cj] - d[ci]|
      - 回転R:      重複カメラそれぞれの姿勢から得た回転の重み付きクォータニオン平均
      - 並進t:      最大重みの重複カメラ(アンカー)が final の位置に一致するよう決定

    weights: {camera_id: 重み}。低confidence(信頼度の低い)カメラの寄与を下げる。
    既定では全カメラを均等に扱う。

    これにより、アンカーカメラは final の位置に留まりつつ、そのファイル内の
    相対ベースラインが共通スケールに揃う。
    """
    cams = sorted(overlap_cams)
    if weights is None:
        weights = {c: 1.0 for c in cams}

    # アンカー: 最も重みの大きいカメラ。
    anchor = max(cams, key=lambda c: weights.get(c, 0.0))

    # 回転: 重み付きクォータニオン平均。
    R = average_rotation(
        [final[c][:3, :3] @ d[c][:3, :3].T for c in cams],
        [weights.get(c, 0.0) for c in cams],
    )

    # スケール: 全ペアのベースライン長の比の重み付き中央値。
    s_list = []
    w_list = []
    for ia in range(len(cams)):
        for ib in range(ia + 1, len(cams)):
            ca, cb = cams[ia], cams[ib]
            base_final = np.linalg.norm(final[cb][:3, 3] - final[ca][:3, 3])
            base_raw = np.linalg.norm(d[cb][:3, 3] - d[ca][:3, 3])
            s_list.append(base_final / base_raw)
            w_list.append(weights.get(ca, 0.0) * weights.get(cb, 0.0))

    if not any(w > 0 for w in w_list):
        # 全重みが0の場合は均等にフォールバック。
        s = sum(s_list) / len(s_list)
    else:
        s = weighted_median(s_list, w_list)

    # アンカーの位置を final に一致させる。
    t = final[anchor][:3, 3] - s * (R @ d[anchor][:3, 3])

    return s, R, t


def repair_low_conf_camera(final, file_poses, i, target_cam, unreliable=()):
    """
    低confidence(信頼度の低い)カメラを含むため信用できない新規カメラ target_cam を、
    隣接トリプレット file_poses[i+1] = (b, c, d) から修復して final に追加する。

    file_poses[i] = (a, b, c) の新規カメラ c(=target_cam) の姿勢が、このファイル内の
    低信頼度カメラ(新規カメラ自身、またはそれが依存する重複カメラ)により信用できない。
    同じカメラ c が正しく推定されている隣接トリプレットから取り直す。

    前提: 全トリプレットの相対カメラ幾何は同一(正規リング)であり、リングの
    1ステップ分のベースライン長は等しい。a, b は既に final で正しく確定済み。

    手順:
      - 隣接トリプレットをカメラ b で final へ回転整列する(R)。
      - スケール s を「final[a] -> final[b]」(=1ステップ長) と
        「next[b] -> next[c]」(隣接内の同ステップ長) の比で決定する。
      - final[b] に留め、c を b からの相対位置・回転で配置する。
    """
    d_failed = file_poses[i]
    cams = sorted(d_failed.keys())
    if target_cam not in d_failed:
        raise ValueError(f"target camera {target_cam} not in triplet {cams}")

    if target_cam in final:
        return  # 既に確定済み(修復不要)

    if i + 1 >= len(file_poses):
        raise ValueError(
            f"low-confidence camera {target_cam} in {cams} has no next triplet to repair from"
        )

    # 隣接トリプレット (b, c, d) 内のアンカー b と修復対象 c の姿勢を使う。
    a, b = cams[0], cams[1]
    c = target_cam

    d_next = file_poses[i + 1]
    next_b = d_next.get(b)
    next_c = d_next.get(c)
    if next_b is None or next_c is None:
        raise ValueError(f"next triplet does not contain repair cameras {b}, {c}")

    final_a = final.get(a)
    final_b = final.get(b)
    if final_a is None or final_b is None:
        raise ValueError(f"anchor cameras {a}, {b} not yet in final for repair")

    # 隣接ファイルのカメラ b の回転を final のカメラ b に揃える回転 R。
    R = final_b[:3, :3] @ next_b[:3, :3].T
    # 隣接ファイル内の b -> c の相対並進(方向)を final フレームへ回転したもの。
    rel_t = next_c[:3, 3] - next_b[:3, 3]
    # スケール: リングの1ステップ長 |final[a] -> final[b]| に揃える。
    s = np.linalg.norm(final_b[:3, 3] - final_a[:3, 3]) / np.linalg.norm(rel_t)

    # final[b] に留め、c を b からの相対位置・回転で配置する。
    final_c = np.eye(4)
    final_c[:3, :3] = R @ next_c[:3, :3]
    final_c[:3, 3] = final_b[:3, 3] + s * (R @ rel_t)
    final[c] = final_c
    print(
        f"REPAIR: camera {c} unreliable (low-confidence cameras "
        f"{sorted(unreliable)} in triplet {cams}); repaired from "
        f"adjacent triplet {sorted(d_next.keys())} (s={s:.4f})"
    )


def ChainMergeMultiCam(cam_pose_csv_list, rel_thr=10.0):
    """
    複数のcamPose CSVを、順次チェーン方式で統合し、最小のcamera_idを
    単位行列とした各カメラの相対姿勢を求める。

    各ファイルの並進tは未知の絶対スケールを持つため、参照カメラ間距離を
    基準にスケールを正規化し、以降はスケールを含む相似変換で接続する。

    各ファイルの per-camera confidence (Jkベース) を参照し、
      - 新規カメラが低信頼度なら隣接トリプレットから修復して確定する。
      - 重複カメラの低信頼度は、相似変換推定でダウンウェイトする。

    処理の流れ:
      1. 最初のファイル camPose_0000_0001_0002.csv の最小camera_id(=0)の
         姿勢M0_0の逆行列を、3つの姿勢に左から掛け、M0a, M1a, M2a とする。
         (M0a=I, M1a=inv(M0_0)@M1_0, M2a=inv(M0_0)@M2_0)
         その後、cam0からcam1へのベースラインを長さ1にスケール正規化する。
      2. 以降のファイルも同様に、重複カメラのベースライン比でスケールを
         揃えながら新規カメラを次々に確定する。

    戻り値: sorted(camera_id) 順の (cam_id, 相対姿勢M) リスト。
    """
    if not cam_pose_csv_list:
        raise ValueError("No input csv files.")

    # 各ファイルを {camera_id: 4x4行列M} として読み込む。
    # 併せて、カメラ行列CSVの追加列から per-camera の Jk / confidence を読み出す。
    file_poses = []
    file_conf = []
    for path in cam_pose_csv_list:
        d = {}
        jk_i = {}
        conf_i = {}
        for cam_id, M, jk, conf in Read_CamPose_CSV(path):
            d[cam_id] = M
            if jk is not None:
                jk_i[cam_id] = jk
            conf_i[cam_id] = conf
        file_poses.append(d)
        # file_conf[i] = ( {camera_id: confidence}, {camera_id: Jk} )
        file_conf.append((conf_i, jk_i))

    # 1. 最初のファイルで最小camera_idを単位行列にする。
    first = file_poses[0]
    first_cams = sorted(first.keys())
    ref_id = first_cams[0]
    T0 = inv_rigid(first[ref_id])

    final = {}
    for cam_id in first_cams:
        final[cam_id] = T0 @ first[cam_id]

    # 参照スケール: cam_ref(原点)から次のカメラまでのベースラインを長さ1にする。
    # (並進は未知の絶対スケールを持つため、この1つのカメラ間距離を基準に正規化)
    c1 = first_cams[1]
    s0 = 1.0 / np.linalg.norm(final[c1][:3, 3])
    for cam_id in final:
        final[cam_id][:3, 3] *= s0

    # 2,3. 以降のファイルを順次チェーンで確定する。
    #     各ファイルで既に確定済みの重複カメラの行列ペア全てを使い、
    #     スケールを含む相似変換Sを頑健に推定して接続する。
    for i in range(1, len(file_poses)):
        d = file_poses[i]
        jk_i = file_conf[i][1]
        conf_i = file_conf[i][0]

        # このファイル内の低信頼度カメラを判定する。
        # (per-camera Jk が同ファイル内の中央値より rel_thr 倍以上大きいカメラ)
        if jk_i:
            med = statistics.median(list(jk_i.values()))
            unreliable = {c for c, j in jk_i.items() if j > rel_thr * med}
        else:
            unreliable = set()

        cams = sorted(d.keys())

        # このファイル内で既に確定済みのカメラ(重複カメラ)。
        overlap_cams = [c for c in cams if c in final]

        # このファイルで新規に追加されるカメラ(まだ final に無いもの)。
        new_cams = [c for c in cams if c not in final]

        # 新規カメラが低信頼度、もしくは新規カメラが依存する重複カメラ(アンカー)が
        # 低信頼度の場合、このファイルの相対幾何は信用できない。
        # → 新規カメラを隣接トリプレットから修復して確定し、このファイルの
        #   劣化した新規カメラ出力は使わない。
        if new_cams and (
            new_cams[0] in unreliable or any(c in unreliable for c in overlap_cams)
        ):
            repair_low_conf_camera(
                final, file_poses, i, new_cams[0], unreliable=unreliable
            )
            continue

        if not overlap_cams:
            raise ValueError(
                f"no overlap camera found in file {cams}. "
                "Feed files in ring order (0000_0001_0002, 0001_0002_0003, ...)."
            )

        # 重複カメラの重み: 低信頼度カメラは相似変換推定から除外(ダウンウェイト)。
        weights = {c: conf_i.get(c, 1.0) for c in overlap_cams}
        for c in overlap_cams:
            if c in unreliable:
                weights[c] = 0.0
        print(
            f"file[{i}] {cams}: unreliable={sorted(unreliable)} "
            f"weights={ {c: round(weights.get(c, 0.0), 4) for c in overlap_cams} }"
        )

        if len(overlap_cams) >= 2:
            # 重複2台以上: 共有ベースラインからスケールを含む相似変換を推定。
            s, R, t = estimate_similarity_transform(final, d, overlap_cams, weights)
        else:
            # 重複1台のみ: スケール情報が得られないので剛体変換で接続(フォールバック)。
            anchor_id = overlap_cams[0]
            R = final[anchor_id][:3, :3] @ d[anchor_id][:3, :3].T
            t = final[anchor_id][:3, 3] - R @ d[anchor_id][:3, 3]
            s = 1.0

        # このファイル内の全カメラ(重複+新規)を、推定した相似変換で共通フレーム
        # へ移す。アンカーカメラは構成上 final の位置に留まり、他カメラは
        # スケール補正された位置に更新される。
        for cam_id in cams:
            final[cam_id] = apply_similarity_to_pose(d[cam_id], s, R, t)

    return sorted(final.items())


def loop_closure_scale_correction(final, wrap_pose_files, L_ref=1.0):
    """
    ループクロージャにより累積スケールドリフトを補正する。

    チェーンマージの結果 final (cam0=I, 辺0->1 を長さ1に正規化) は、スケール
    推定誤差がリング一周で累積し、閉ループ辺(23->0)の長さが本来の値から
    ずれる(ドリフト)。

    折り返しトリプレット(0022_0023_0000, 0023_0000_0001)のうち、参照辺 0->1 と
    閉ループ辺 23->0 を同一スケールで持つファイルから真の閉ループ辺長 L_true を求め、
    チェーンの辺を滑らかに(線形ランプ)スケール補正してリングを閉じる。
    回転は保持し、並進(位置)のみ補正する。
    """
    cams = sorted(final.keys())
    if 0 not in cams or 23 not in cams:
        return final  # リングでない場合は補正しない

    # 1. 参照辺 0->1 と閉ループ辺 23->0 を同一スケールで持つ折り返しファイルから
    #    真の閉ループ辺長 L_true を求める。
    L_true = None
    for path in wrap_pose_files:
        d = {}
        for cam_id, M, _jk, _cf in Read_CamPose_CSV(path):
            d[cam_id] = M
        if 0 in d and 1 in d and 23 in d:
            e_ref = np.linalg.norm(d[1][:3, 3] - d[0][:3, 3])
            e_close = np.linalg.norm(d[23][:3, 3] - d[0][:3, 3])
            if e_ref > 1e-12:
                L_true = L_ref * (e_close / e_ref)
                break
    if L_true is None:
        L_true = L_ref  # フォールバック: 正規リングと仮定

    # 2. チェーンの閉ループ辺長(ドリフトした値)。
    l_chain = np.linalg.norm(final[23][:3, 3] - final[0][:3, 3])

    if abs(l_chain - L_true) < 1e-9 * L_ref:
        print(
            f"loop closure: closing edge already {l_chain:.4f} ~ L_true {L_true:.4f}; no correction"
        )
        return final

    # 3. 位置を「放射方向」に滑らかにスケール補正する。
    #    累積スケールドリフトは乗算的(約2倍)で、線形な辺ランプでは閉じられないため、
    #    各カメラの原点からの位置ベクトルを g_k = 1 + c*(k/23) でスケールする。
    #    p'_k = g_k * p_k は方向を保ち、回転行列はそのまま保持する。
    #    閉ループ条件 |p'_23| = L_true から c は一意に決まり、常に解が存在する:
    #        |(1 + c) * p_23| = L_true  =>  c = L_true / |p_23| - 1
    N = 23  # カメラ総数(0..22がチェーン、23が閉ループ端)
    r = np.linalg.norm(final[N][:3, 3])
    if r <= 1e-12:
        print("loop closure: closing camera at origin; no correction")
        return final
    c = L_true / r - 1.0

    # 4. 位置を再構成(回転はfinalのまま、並進のみ放射方向にスケール)。
    new_final = {}
    for k in range(N + 1):
        g = 1.0 + c * (k / N)
        M = final[k].copy()
        M[:3, 3] = g * final[k][:3, 3]
        new_final[k] = M

    print(
        f"loop closure: closing edge {l_chain:.4f} -> L_true {L_true:.4f} "
        f"(drift ratio {l_chain / L_true:.4f}); applied radial scale correction "
        f"(factor range 1.0 -> {1.0 + c:.4f})"
    )
    return new_final


def Run_MergeMultiCam(
    in_cam_pose_csv_list,
    out_cam_pose_csv,
    out_cam_pose_ply=None,
    loop_closure_csv_list=None,
):
    # 各ファイルの per-camera confidence(Jkベース) に従い、低信頼度カメラを
    # ダウンウェイト / 隣接トリプレットから修復してマージする。
    merged = ChainMergeMultiCam(in_cam_pose_csv_list)

    # ループクロージャ: 折り返しトリプレットで累積スケールドリフトを補正する。
    if loop_closure_csv_list:
        merged_dict = dict(merged)
        merged_dict = loop_closure_scale_correction(merged_dict, loop_closure_csv_list)
        merged = sorted(merged_dict.items())

    cam_id_list = [cam_id for cam_id, _ in merged]
    t_list = [M[:3, 3].reshape(3, 1) for _, M in merged]
    R_list = [M[:3, :3] for _, M in merged]

    CSV_Write_CamPose_list(out_cam_pose_csv, t_list, R_list, cam_id_list)
    if out_cam_pose_ply:
        PLY_Export_MultiCam(out_cam_pose_ply, t_list, R_list)

    return True


def main():
    parser = argparse.ArgumentParser(
        description="merge multiple camPose CSVs into one relative-to-min-camera_id pose"
    )
    parser.add_argument(
        "--cam_pose_csv",
        nargs="+",
        default=None,
        help="input camPose csv files (space separated). "
        "If omitted, files are auto-generated from --dir/--prefix/--start/--cam_num.",
    )
    parser.add_argument(
        "--dir",
        type=str,
        default="tmp",
        help="directory of camPose csv files (used when --cam_pose_csv is omitted).",
    )
    parser.add_argument(
        "--prefix",
        type=str,
        default="camPose",
        help="file name prefix (used when --cam_pose_csv is omitted).",
    )
    parser.add_argument(
        "--start",
        type=int,
        default=0,
        help="first camera triplet index (used when --cam_pose_csv is omitted).",
    )
    parser.add_argument(
        "--cam_num",
        type=int,
        default=None,
        help="number of camPose csv files to merge (used when --cam_pose_csv is omitted). "
        "For the 24-camera ring use 22.",
    )
    parser.add_argument(
        "--out_cam_pose_csv",
        type=str,
        default="tmp/camPose_merged.csv",
        help="output merged camPose csv",
    )
    parser.add_argument(
        "--out_cam_pose_ply",
        type=str,
        default=None,
        help="output merged camPose ply (optional)",
    )
    parser.add_argument(
        "--loop_closure_csv",
        nargs="+",
        default=None,
        help="wrap-around triplet camPose csv files (space separated) used to "
        "correct cumulative scale drift by loop closure (optional).",
    )
    parser.add_argument(
        "--bundle_adjustment",
        action="store_true",
        help="run global ring bundle adjustment after merging (optional).",
    )
    parser.add_argument(
        "--f0",
        type=float,
        default=None,
        help="initial focal length for bundle adjustment (default: auto from feature spread).",
    )
    parser.add_argument(
        "--out_cam_pose_ba_csv",
        type=str,
        default="tmp/camPose_ba.csv",
        help="output bundle-adjusted merged camPose csv",
    )
    parser.add_argument(
        "--out_cam_pose_ba_ply",
        type=str,
        default=None,
        help="output bundle-adjusted merged camPose ply (optional)",
    )
    parser.add_argument(
        "--out_points3d_ba_csv",
        type=str,
        default=None,
        help="output bundle-adjusted 3D points csv (optional)",
    )
    parser.add_argument(
        "--out_points3d_ba_ply",
        type=str,
        default=None,
        help="output bundle-adjusted 3D points ply (optional)",
    )
    args = parser.parse_args()

    if args.cam_pose_csv is None:
        if args.cam_num is None:
            parser.error("either --cam_pose_csv or --cam_num must be given")
        args.cam_pose_csv = Build_RangeCamPose_Files(
            args.dir, args.prefix, args.start, args.cam_num
        )

    br = Run_MergeMultiCam(
        args.cam_pose_csv,
        args.out_cam_pose_csv,
        args.out_cam_pose_ply,
        loop_closure_csv_list=args.loop_closure_csv,
    )
    if br == True:
        rv = 0
    else:
        rv = 1

    print(f"merged {len(args.cam_pose_csv)} csv files -> {args.out_cam_pose_csv}")

    if args.bundle_adjustment and rv == 0:
        br = Run_BundleAdjustment(
            args.cam_pose_csv,
            args.out_cam_pose_csv,
            args.out_cam_pose_ba_csv,
            out_cam_pose_ply=args.out_cam_pose_ba_ply,
            out_points3d_csv=args.out_points3d_ba_csv,
            out_points3d_ply=args.out_points3d_ba_ply,
            f0=args.f0,
        )
        if br == True:
            rv = 0
        else:
            rv = 1
        print(
            f"bundle adjusted {len(args.cam_pose_csv)} csv files -> "
            f"{args.out_cam_pose_ba_csv}"
        )

    sys.exit(rv)


if __name__ == "__main__":
    import sys

    main()
