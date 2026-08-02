from abc import ABC, abstractmethod
from Common import *


class RegressorTwoCamBase(ABC):
    @abstractmethod
    def fit_TwoCam(pp: Point2dPair):
        pass

    @abstractmethod
    def calc_loss_tc(pp: Point2dPair):
        pass

    @abstractmethod
    def get_theta():
        pass

