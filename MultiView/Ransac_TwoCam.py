# https://en.wikipedia.org/wiki/Random_sample_consensus
from copy import copy
import numpy as np
from numpy.random import default_rng
from abc import ABC, abstractmethod

rng = default_rng()

class RegresserBase_TwoCam(ABC):
    @abstractmethod
    def fit_tc(x_list,y_list):
        pass

    @abstractmethod
    def get_theta():
        pass


class Ransac_TwoCam:
    def __init__(self, ite_count=100, loss_threshold=1e-11, close_points=100, model=RegresserBase_TwoCam):
        self.ite_count = ite_count              # Maximum iterations allowed
        self.loss_threshold = loss_threshold              # `Threshold value to determine if points are fit well
        self.close_points = close_points              # Number of close data points required to assert model fits well
        self.model = model      # `model`: class implementing `fit` and `predict`
        self.best_model = None
        self.best_loss = np.inf
        self.inlier_flag_list = None
        self.inlier_core_flag_list = None

    def get_theta(self):
        return self.best_model.get_theta()
    
    def get_inlier_flag_list(self):
        return self.inlier_flag_list

    def get_inlier_core_flag_list(self):
        return self.inlier_core_flag_list

    def fit(self, a: np.ndarray, b: np.ndarray):
        MIN_POINT_COUNT = 8 # do not change, Minimum number of data points to estimate parameters

        N = a.shape[0]
        assert N == b.shape[0]

        for i in range(self.ite_count):
            ids = rng.permutation(a.shape[0])

            # self.n == 8点 picked up
            maybe_inliers = ids[: MIN_POINT_COUNT]
            maybe_model = copy(self.model)
            ai = a[maybe_inliers, :]
            bi = b[maybe_inliers, :]
            maybe_model.fit_tc(xy0_list=ai, xy1_list=bi)

            # calc loss with all the other points
            ids_the_other = ids[MIN_POINT_COUNT :]
            loss_list = maybe_model.calc_loss(a[ids_the_other, :], b[ids_the_other, :])

            thresholded = (
                loss_list < self.loss_threshold
            )

            inlier_ids = ids_the_other[np.flatnonzero(thresholded)]

            inlier_ids_size = inlier_ids.size

            if self.close_points <= inlier_ids_size:
                inlier_points = np.hstack([maybe_inliers, inlier_ids])
                a2=a[inlier_points, :]
                b2=b[inlier_points, :]
                better_model = copy(self.model)
                better_model.fit_tc(a2, b2)

                this_loss = better_model.calc_loss(a2, b2)
                this_loss_mean = this_loss.mean()
                this_loss_max = this_loss.max()

                if this_loss_mean < self.best_loss:
                    print(f"  i={i}/{self.ite_count} found better set. mean_loss {this_loss_mean}, max_loss {this_loss_max}, good points={inlier_ids_size + MIN_POINT_COUNT}")
                    self.best_loss  = this_loss_mean
                    self.best_model = better_model
                    
                    self.inlier_flag_list=N * [False]
                    for id in inlier_ids:
                        self.inlier_flag_list[id] = True

                    self.inlier_core_flag_list = N * [False]
                    for id in maybe_inliers:
                        self.inlier_core_flag_list[id] = True
                        self.inlier_flag_list[id] = True

                else:
                    #print(f"  i={i} inliner count is OK {inlier_ids_size} > {self.close_points}, but error is large {this_loss} > {self.best_loss}")
                    pass
            else:
                print(f"  i={i} did not met inliers count condition {self.close_points} <= {inlier_ids_size}.")
                pass

        return self

    def calc_loss(self, a, b):
        return self.best_model.calc_loss(a, b)

