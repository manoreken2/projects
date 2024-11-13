import numpy as np


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







