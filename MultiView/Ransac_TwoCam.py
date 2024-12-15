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
    def __init__(self, n=8, k=100, t=0.01, d=100, model=RegresserBase_TwoCam):
        self.n = n              # `n`: Minimum number of data points to estimate parameters
        self.k = k              # `k`: Maximum iterations allowed
        self.t = t              # `t`: Threshold value to determine if points are fit well
        self.d = d              # `d`: Number of close data points required to assert model fits well
        self.model = model      # `model`: class implementing `fit` and `predict`
        self.best_model = None
        self.best_loss = np.inf
        self.inlier_ids = []
        self.c_list = None

    def get_theta(self):
        return self.best_model.get_theta()
    
    def get_c_list(self):
        return self.c_list

    def fit(self, a: np.ndarray, b: np.ndarray):
        N = a.shape[0]
        assert N == b.shape[0]

        for i in range(self.k):
            ids = rng.permutation(a.shape[0])

            # self.n == 8点 picked up
            maybe_inliers = ids[: self.n]
            maybe_model = copy(self.model)
            ai = a[maybe_inliers, :]
            bi = b[maybe_inliers, :]
            maybe_model.fit_tc(xy0_list=ai, xy1_list=bi)

            # calc loss with all the other points
            ids_the_other = ids[self.n :]
            loss_list = maybe_model.calc_loss(a[ids_the_other, :], b[ids_the_other, :])

            thresholded = (
                loss_list < self.t
            )

            inlier_ids = ids_the_other[np.flatnonzero(thresholded)]

            inlier_ids_size = inlier_ids.size

            if self.d <= inlier_ids_size:
                inlier_points = np.hstack([maybe_inliers, inlier_ids])
                a2=a[inlier_points, :]
                b2=b[inlier_points, :]
                better_model = copy(self.model)
                better_model.fit_tc(a2, b2)

                this_loss = better_model.calc_loss(a2, b2).mean()

                if this_loss < self.best_loss:
                    print(f"  i={i}/{self.k} found better set. loss {this_loss}, inliers={inlier_ids_size}")
                    self.best_loss  = this_loss
                    self.best_model = better_model
                    self.inlier_ids = inlier_ids

                    # c_list[id] == 0 : inlier
                    # c_list[id] == 1 : outlier
                    self.c_list=N * [1]
                    for id in self.inlier_ids:
                        self.c_list[id] = 0
                else:
                    print(f"  i={i} inliner count is OK {inlier_ids_size} > {self.d}, but error is large {this_loss} > {self.best_loss}")
                    pass
            else:
                print(f"  i={i} did not met inliers count condition {self.d} <= {inlier_ids_len}. loss={loss_list.mean()}")
                pass

        return self

    def calc_loss(self, a, b):
        return self.best_model.calc_loss(a, b)

