# https://en.wikipedia.org/wiki/Random_sample_consensus
from copy import copy
import numpy as np
from numpy.random import default_rng
from abc import ABC, abstractmethod

rng = default_rng()

class RegresserBase(ABC):
    @abstractmethod
    def get_theta():
        pass

    #@abstractmethod
    #def get_w_list():
    #    pass

class Ransac:
    def __init__(self, n=10, k=100, t=0.05, d=10, model=RegresserBase):
        self.n = n              # `n`: Minimum number of data points to estimate parameters
        self.k = k              # `k`: Maximum iterations allowed
        self.t = t              # `t`: Threshold value to determine if points are fit well
        self.d = d              # `d`: Number of close data points required to assert model fits well
        self.model = model      # `model`: class implementing `fit` and `predict`
        self.best_model = None
        self.best_loss = np.inf
        self.inlier_ids = None
        self.c_list = None

    def get_theta(self):
        return self.best_model.get_theta()

    def get_w_list(self):
        return self.best_model.get_w_list()
    
    def get_c_list(self):
        return self.c_list

    def fit(self, X: np.ndarray, Y: np.ndarray):
        N = X.shape[0]
        assert N == Y.shape[0]

        for i in range(self.k):
            ids = rng.permutation(X.shape[0])

            maybe_inliers = ids[: self.n]
            maybe_model = copy(self.model).fit(X[maybe_inliers], Y[maybe_inliers])

            loss_list = maybe_model.calc_loss(X[ids][self.n :], Y[ids][self.n :])

            thresholded = (
                loss_list < self.t
            )

            inlier_ids = ids[self.n :][np.flatnonzero(thresholded).flatten()]

            if inlier_ids.size > self.d:
                inlier_points = np.hstack([maybe_inliers, inlier_ids])
                better_model = copy(self.model).fit(X[inlier_points], Y[inlier_points])

                this_loss = better_model.calc_loss(X[inlier_points], Y[inlier_points]).mean()

                if this_loss < self.best_loss:
                    print(f"D: i={i}/{self.k} loss {this_loss}, inliers={inlier_ids.size}")
                    self.best_loss = this_loss
                    self.best_model = better_model
                else:
                    #print(f"D: inliner count is OK {inlier_ids.size}, but error is large {this_loss}")
                    pass
            else:
                #print(f"D: not met inliers count condition {inlier_ids.size} < {self.d}. loss={loss_list.mean()}")
                pass

        self.inlier_ids = inlier_ids

        # c_list[id] == 0 : inlier 
        # c_list[id] == 1 : outlier
        self.c_list=N * [1]
        for id in inlier_ids:
            self.c_list[id] = 0

        return self

    def calc_loss(self, x_list, y_list):
        return self.best_model.calc_loss(x_list, y_list)

