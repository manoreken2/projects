# https://en.wikipedia.org/wiki/Random_sample_consensus
from copy import copy
import os
import multiprocessing
import numpy as np
from numpy.random import default_rng
from abc import ABC, abstractmethod
from Common import *
from Fundamental_to_CamParams import *
from RegressorTwoCamBase import RegressorTwoCamBase


def _fit_chunk(
    pp, model, ite_start, ite_end, seed, loss_threshold, close_points_ratio,
    MIN_POINT_COUNT,
):
    rng = default_rng(seed)
    N = pp.get_point_count()

    best_loss = np.inf
    best_model = None
    best_inlier_ids = None
    valid_bitmap = None
    picked_up_ids = None

    for i in range(ite_start, ite_end):
        ids = rng.permutation(N)

        # self.n == 8点 picked up
        picked_up = ids[:MIN_POINT_COUNT]
        maybe_model = copy(model)
        r = maybe_model.fit_TwoCam(
            Point2dPair(pp.a[picked_up, :], pp.b[picked_up, :])
        )
        if r is None:
            continue

        # calc loss with all the other points
        the_other_ids = ids[MIN_POINT_COUNT:]
        loss_list = maybe_model.calc_loss_tc(
            Point2dPair(pp.a[the_other_ids, :], pp.b[the_other_ids, :])
        )

        thresholded = loss_list < 2 * loss_threshold * loss_threshold
        inlier_ids = the_other_ids[np.flatnonzero(thresholded)]

        if close_points_ratio * N <= inlier_ids.size:
            valid_ids = np.hstack([picked_up, inlier_ids])
            valid_pp = Point2dPair(pp.a[valid_ids, :], pp.b[valid_ids, :])
            candidate = copy(model)
            r = candidate.fit_TwoCam(valid_pp)
            if r is None:
                continue

            this_loss_mean = candidate.calc_loss_tc(valid_pp).mean()

            if this_loss_mean < best_loss:
                best_loss = this_loss_mean
                best_model = candidate
                best_inlier_ids = inlier_ids
                bitmap = N * [False]
                for id in valid_ids:
                    bitmap[id] = True
                valid_bitmap = bitmap
                picked_up_ids = picked_up

    if best_model is None:
        return None
    return (best_loss, best_model, best_inlier_ids, valid_bitmap, picked_up_ids)


# 手順3.8 p.53
class Ransac_TwoCam:
    def __init__(
        self,
        f0=600,
        ite_count=1000,
        loss_threshold=5.0,
        close_points_ratio=0.8,
        model=RegressorTwoCamBase,
        num_workers=None,
    ):
        self.ite_count = ite_count  # Maximum iterations allowed
        self.loss_threshold = (
            loss_threshold  # `Threshold value to determine if points are fit well
        )
        self.close_points_ratio = close_points_ratio
        self.best_model = None
        self.best_loss = np.inf
        self.valid_bitmap = None
        self.best_inlier_ids = None
        self.picked_up_ids = None
        self.f0 = f0
        self.model = model
        self.num_workers = (
            num_workers
            if num_workers is not None
            else max(1, os.cpu_count() // 2)
        )

    def get_theta(self):
        return self.best_model.get_theta()

    def get_valid_bitmap(self):
        return self.valid_bitmap

    # RANSACが取り出した、正しい8点。
    def get_picked_up_ids(self):
        return self.picked_up_ids

    def get_best_loss(self):
        return self.best_loss

    def fit(self, pp: Point2dPair):
        MIN_POINT_COUNT = (
            8  # do not change, Minimum number of data points to estimate parameters
        )

        ite_count = self.ite_count
        n_workers = max(1, min(self.num_workers, ite_count))

        # Split the iteration range among worker processes.
        base, rem = divmod(ite_count, n_workers)
        seed_rng = default_rng()
        tasks = []
        start = 0
        for w in range(n_workers):
            count = base + (1 if w < rem else 0)
            tasks.append(
                (
                    pp,
                    self.model,
                    start,
                    start + count,
                    int(seed_rng.integers(0, 2**31)),
                    self.loss_threshold,
                    self.close_points_ratio,
                    MIN_POINT_COUNT,
                )
            )
            start += count

        with multiprocessing.Pool(processes=n_workers) as pool:
            results = pool.starmap(_fit_chunk, tasks)

        for r in results:
            if r is None:
                continue
            loss, model, inlier_ids, valid_bitmap, picked_up_ids = r
            if loss < self.best_loss:
                self.best_loss = loss
                self.best_model = model
                self.best_inlier_ids = inlier_ids
                self.valid_bitmap = valid_bitmap
                self.picked_up_ids = picked_up_ids

        if self.best_model == None:
            return None

        print(
            f"Ransac_TwoCam.fit() best inliers {self.best_inlier_ids.size} best Error {self.best_loss}"
        )

        return self

    def calc_loss_tc(self, pp: Point2dPair):
        return self.best_model.calc_loss_tc(pp)
