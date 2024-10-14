import matplotlib.pyplot as plt
import csv
import numpy as np
from numpy.linalg import eigh
import math
from Common import ReadPointXY3, Plot, BuildXi, BuildV0, BuildM, BuildL
from Ransac import Ransac, RegresserBase
from FNSEllipRegressor import FNSEllipRegressor


def main():
    f0=1
    MaxIter=100
    ConvEPS=1e-5
    SampleCount=6 # smallest point count to get ellipsis params

    print("a")

    x_list, y_list = ReadPointXY3('pointXY2.csv')
    N=x_list.shape[0]
    assert N == y_list.shape[0]

    reg = Ransac(model=FNSEllipRegressor(MaxIter, ConvEPS, f0), n=SampleCount, t=1.0, d=N*0.8, k=300)
    reg.fit(x_list, y_list)

    theta = reg.get_theta()
    c_list = reg.get_c_list()

    Plot(theta, c_list, f0, x_list, y_list)            


if __name__ == "__main__":
    main()
