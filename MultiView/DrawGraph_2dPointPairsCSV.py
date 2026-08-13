"""
DrawGraph_2dPointPairsCSV.py

Reads a CSV of matched 2D point pairs (2 or more cameras) and draws them as a
scatter + connecting-lines plot, saving the result as a PNG.

Usage
-----
    python DrawGraph_2dPointPairsCSV.py [csv_path] [--out_png PATH] [--no-swap-axes]

Arguments
---------
    csv_path        CSV file of matched 2D point pairs.
                    Default: tmp/op0001_0002.csv
    --out_png PATH  Output PNG path.
                    Default: tmp/DrawGraph_2dPointPairsCSV.png
    --no-swap-axes  Disable axis swap. Axes are swapped (x<->y) by default.

Supported CSV formats (auto-detected from the header)
-----------------------------------------------------
1) Legacy 2-camera CSV (e.g. tmp/op0001_0002.csv):
       x1(right+), y1(down+), x2, y2, Lowes_ratio, reproj_error_px
   The two point pairs are in the first 4 columns; the remaining columns
   (Lowes_ratio, reproj_error_px) are metadata and are ignored.

2) Generic N-camera CSV (e.g. tmp/op0001_0002_0003.csv):
       x0, y0, x1, y1, x2, y2, ...
   Columns are interleaved x_i, y_i, one pair per camera. The number of
   cameras is derived as n_cams = (number of columns) / 2.

Examples
--------
    python DrawGraph_2dPointPairsCSV.py
    python DrawGraph_2dPointPairsCSV.py tmp/op0001_0002_0003.csv
    python DrawGraph_2dPointPairsCSV.py tmp/op0001_0002_0003.csv --no-swap-axes
    python DrawGraph_2dPointPairsCSV.py tmp/op0001_0002_0003.csv --out_png tmp/g.png
"""

import os
import argparse

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def main(
    csv_path: str,
    out_png: str = "DrawGraph_2dPointPairsCSV.png",
    swap_axes: bool = False,
):
    df = pd.read_csv(csv_path)
    cols = [c.strip() for c in df.columns]

    # Legacy 2-cam CSV (e.g. tmp/op0001_0002.csv) carries metadata columns
    # (Lowes_ratio, reproj_error_px) after the two point pairs.
    legacy_2cam = any(("Lowes" in c) or ("reproj" in c) for c in cols)

    if legacy_2cam:
        n_cams = 2
        xs = [df.iloc[:, 0], df.iloc[:, 2]]
        ys = [df.iloc[:, 1], df.iloc[:, 3]]
    else:
        # Interleaved x_i, y_i columns, one pair per camera (e.g. 3-cam CSV).
        n_cams = df.shape[1] // 2
        xs = [df.iloc[:, 2 * i] for i in range(n_cams)]
        ys = [df.iloc[:, 2 * i + 1] for i in range(n_cams)]

    xs = [np.asarray(c, dtype=float) for c in xs]
    ys = [np.asarray(c, dtype=float) for c in ys]

    label_x = "x"
    label_y = "y"

    if swap_axes:
        for i in range(n_cams):
            xs[i], ys[i] = ys[i], xs[i]
        label_x = "y"
        label_y = "x"

    fig, ax = plt.subplots(figsize=(8, 8))

    cmap = plt.get_cmap("tab10")
    markers = ["o", "s", "^", "D", "x", "P"]

    for i in range(n_cams):
        ax.scatter(
            xs[i],
            ys[i],
            marker=markers[i % len(markers)],
            color=cmap(i % 10),
            s=18,
            label=f"point {i}",
        )

    n_rows = len(xs[0])
    for r in range(n_rows):
        ax.plot(
            [xs[i][r] for i in range(n_cams)],
            [ys[i][r] for i in range(n_cams)],
            color="tab:gray",
            linewidth=0.5,
            alpha=0.6,
        )

    ax.axhline(0, color="k", linewidth=0.8)
    ax.axvline(0, color="k", linewidth=0.8)

    x_max = max(np.abs(ax.get_xlim())[0], np.abs(ax.get_xlim())[1])
    y_max = max(np.abs(ax.get_ylim())[0], np.abs(ax.get_ylim())[1])
    ax.annotate(
        "",
        xy=(x_max * 1.15, 0),
        xytext=(0, 0),
        arrowprops=dict(arrowstyle="->", color="k", lw=1.2),
    )
    ax.text(x_max * 1.18, 0, "x", ha="left", va="center")
    ax.annotate(
        "",
        xy=(0, y_max * 1.15),
        xytext=(0, 0),
        arrowprops=dict(arrowstyle="->", color="k", lw=1.2),
    )
    ax.text(0, y_max * 1.18, "y", ha="center", va="bottom")

    ax.set_xlim(-x_max * 1.3, x_max * 1.3)
    ax.set_ylim(-y_max * 1.3, y_max * 1.3)
    ax.set_aspect("equal", adjustable="box")
    ax.grid(True, linestyle=":", alpha=0.5)
    ax.set_xlabel(label_x)
    ax.set_ylabel(label_y)
    ax.set_title(
        f"2D point pairs ({n_cams} cams)" + (" (axes swapped)" if swap_axes else "")
    )
    ax.legend(loc="best")

    fig.tight_layout()
    fig.savefig(out_png, dpi=150)
    print(f"saved: {os.path.abspath(out_png)}")
    plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Draw 2D point pairs of two or more cameras from a CSV file"
    )
    parser.add_argument(
        "csv_path",
        nargs="?",
        default="tmp/op0001_0002.csv",
        help="CSV file of matched 2D point pairs (default: tmp/op0001_0002.csv)",
    )
    parser.add_argument(
        "--out_png",
        type=str,
        default="tmp/DrawGraph_2dPointPairsCSV.png",
        help="output PNG path (default: tmp/DrawGraph_2dPointPairsCSV.png)",
    )
    parser.add_argument(
        "--no-swap-axes",
        dest="swap_axes",
        action="store_false",
        help="do not swap x and y axes for display (swapping is on by default)",
    )
    parser.set_defaults(swap_axes=True)
    args = parser.parse_args()

    print(f"in_csv={args.csv_path} out_png={args.out_png} swap={args.swap_axes}")

    main(args.csv_path, args.out_png, swap_axes=args.swap_axes)
