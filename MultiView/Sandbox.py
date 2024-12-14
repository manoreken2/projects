import numpy as np
from Common import *

M = np.array([ \
    [1, 2, 3],
    [4, 5, 6],
    [7, 8, 9]
    ])

# メモリの並びは行優先。
# [ 1 2 3 ]
# [ 4 5 6 ]
# [ 7 8 9 ]

x = np.array([ \
    [1],
    [1],
    [0]])

r = M @ x

# r: Mx = [3, 9, 15]^T

print(f"m={M}")
print(f"x={x}")
print(f"r={r}")




from numpy.random import default_rng

rng = default_rng()
p = rng.permutation(4)

print(f"{p}")

loss_list = np.array([ 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, ])

thr = ( loss_list < 0.1 )
# thr = [ F, T, F, T, F, T ]
#         0  1  2  3  4  5

tfnz = np.flatnonzero(thr)
# tfnz = [ 1, 3, 5 ] # True のidx番号

tff = tfnz.flatten()
# no effect

#inlier_ids = p[2 :][tff]

#print(f"{inlier_ids}")

count_up = np.array([ 0, 1, 2, 3, 4, 5, 6 ])

c2 = count_up[ : 3 ]
print(f"{c2}")
# c2 = [ 0, 1, 2 ]

c3 = count_up[ 3 : ]
# c3 = [ 3, 4, 5, 6 ]

c4 = count_up[ 3 : ][ : 2 ]
# c4 = [ 3, 4 ]


print("")


xy0 = np.array([1,2])

# creates copy
xy1 = np.array(xy0)

xy0[0]=3

print(f"0={xy0}, 1={xy1}")

xy0_list, xy1_list = CSV_Read_TwoCamPointList('twoCamPoints410_outlier10.csv')
xy0 = xy0_list[0,:]
print(f"xy0={xy0}")


v0 = np.vstack([1,2])
v1 = np.vstack([3,4])

s = np.vdot(v0,v1)
print(f"s={s}")









