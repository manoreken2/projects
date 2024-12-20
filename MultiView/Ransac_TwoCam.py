# https://en.wikipedia.org/wiki/Random_sample_consensus
from copy import copy
import numpy as np
from numpy.random import default_rng
from abc import ABC, abstractmethod
from Common import *

rng = default_rng()

class RegresserBase_TwoCam(ABC):
    @abstractmethod
    def fit_tc(pp: Point2dPair):
        pass

    @abstractmethod
    def calc_loss_tc(pp: Point2dPair):
        pass

    @abstractmethod
    def get_theta():
        pass


class Ransac_TwoCam:
    def __init__(self, ite_count=100, loss_threshold=1e-11, close_points=100, model=RegresserBase_TwoCam):
        self.ite_count = ite_count           # Maximum iterations allowed
        self.loss_threshold = loss_threshold # `Threshold value to determine if points are fit well
        self.close_points = close_points     # Number of close data points required to assert model fits well
        self.model = model
        self.best_model = None
        self.best_loss = np.inf
        self.valid_bitmap = None
        self.picked_up_ids = None

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
        MIN_POINT_COUNT = 8 # do not change, Minimum number of data points to estimate parameters

        N = pp.get_point_count()

        for i in range(self.ite_count):
            ids = rng.permutation(N)

            # self.n == 8点 picked up
            picked_up_ids = ids[: MIN_POINT_COUNT]
            maybe_model   = copy(self.model)
            maybe_model.fit_tc(Point2dPair(pp.a[picked_up_ids, :], pp.b[picked_up_ids, :]))

            # calc loss with all the other points
            the_other_ids = ids[MIN_POINT_COUNT :]
            loss_list     = maybe_model.calc_loss_tc(Point2dPair(pp.a[the_other_ids, :], pp.b[the_other_ids, :]))

            thresholded = (
                loss_list < self.loss_threshold
            )

            inlier_ids = the_other_ids[np.flatnonzero(thresholded)]

            if self.close_points <= inlier_ids.size:
                valid_ids = np.hstack([picked_up_ids, inlier_ids])
                valid_pp  = Point2dPair(pp.a[valid_ids, :], pp.b[valid_ids, :])
                candidate = copy(self.model)
                candidate.fit_tc(valid_pp)

                this_loss      = candidate.calc_loss_tc(valid_pp)
                this_loss_mean = this_loss.mean()
                this_loss_max  = this_loss.max()

                if this_loss_mean < self.best_loss:
                    print(f"  i={i}/{self.ite_count} found better set. mean_loss {this_loss_mean}, max_loss {this_loss_max}, valid point count {valid_ids.size}")
                    self.best_loss  = this_loss_mean
                    self.best_model = candidate
                    
                    self.valid_bitmap = N * [False]
                    for id in valid_ids:
                        self.valid_bitmap[id] = True

                    self.picked_up_ids = picked_up_ids

                else:
                    #print(f"  i={i} inliner count is OK {inlier_ids.size} > {self.close_points}, but error is large {this_loss} > {self.best_loss}")
                    pass
            else:
                #print(f"  i={i} did not met inliers count condition {self.close_points} <= {inlier_ids.size}.")
                pass

        return self

    def calc_loss(self, pp: Point2dPair):
        return self.best_model.calc_loss_tc(pp)

