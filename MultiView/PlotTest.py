import matplotlib.pyplot as plt
import numpy as np
from Common import *

def main():
    t = np.array((1,0,0))
    R = RotY(np.pi /6.0)

    M = Trans_Rot_to_TransformMat(t, R)

    p0, v0 = Generate_CameraMesh()
    p1 = Transform_PointList(p0, M)
    
    p,v = MergeMesh(p0,v0, p1, v0)


    Export_Ply(p,v, 'test.ply')



if __name__ == "__main__":
    main()
