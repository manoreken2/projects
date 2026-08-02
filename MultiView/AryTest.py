import numpy as np

M = np.eye(4)
M[0,3] = 1
M[1,3] = 2
M[2,3] = 3
M[3,3] = 1

p = np.vstack([10,20,30,1])

p1 = M @ p

print(p1) # OK





