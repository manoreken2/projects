import matplotlib.pyplot as plt
import csv
import numpy as np
from numpy.linalg import eigh
import math

def BuildXi(x_list: np.ndarray, y_list: np.ndarray, f0):
    N = x_list.shape[0]
    assert N == y_list.shape[0]

    xi_list = []
    for i in range(N):
        x = x_list[i]
        y = y_list[i]

        xi=np.vstack([x*x, 2*x*y, y*y, 2*f0*x, 2*f0*y, f0*f0])
        xi_list.append(xi)
    return xi_list

def BuildV0(x_list: np.ndarray, y_list: np.ndarray, f0):
    N = x_list.shape[0]
    assert N == y_list.shape[0]

    v0_list = []
    for i in range(N):
        x = x_list[i]
        y = y_list[i]

        v0=np.zeros((6,6))
        v0[0,0] = 4*x*x
        v0[0,1] = v0[1,0] = v0[1,2] = v0[2,1] = 4*x*y
        v0[1,1] = 4* (x*x+y*y)
        v0[2,2] = 4*y*y
        v0[0,3] = v0[3,0] = v0[1,4] = v0[4,1] = 4*f0*x
        v0[1,3] = v0[3,1] = v0[2,4] = v0[4,2] = 4*f0*y
        v0[3,3] = v0[4,4] = 4*f0*f0
        v0_list.append(v0)
    return v0_list

# 対称行列M
def BuildM(xi_list, w_list: np.ndarray):
    N = len(xi_list)
    assert N == w_list.shape[0]

    M = np.zeros((6, 6))
    for i in range(N):
        xi = xi_list[i]
        w  = w_list[i]

        a =  (w/N) * xi @ xi.T
        M += a
    return M

# 対称行列L
def BuildL(xi_list, w_list: np.ndarray, v0_list, ev):
    N = len(xi_list)
    assert N == w_list.shape[0]
    assert N == len(v0_list)

    L = np.zeros((6, 6))
    for i in range(N):
        xi = xi_list[i]
        w  = w_list[i]
        v0 = v0_list[i]
        xi_dot_ev = (xi.T @ ev).item()

        a = (w*w/N) * xi_dot_ev * xi_dot_ev * v0
        L += a
    return L

def ReadPointXY3(path):
    x_list=[]
    y_list=[]
    with open(path) as f:
        r = csv.reader(f, quoting=csv.QUOTE_NONNUMERIC, delimiter=',')
        for l in r:
            x_list.append(l[0])
            y_list.append(l[1])
    return np.asarray(x_list), np.asarray(y_list)

# フィットした楕円を図示。
def Plot(ev, w_list: np.ndarray, f0, x_list: np.ndarray, y_list: np.ndarray):
    # 楕円方程式の係数。
    # Ax^2 + 2Bxy + Cy^2 + 2f0(Dx + Ey) + f0^2*F = 0

    ev = ev.reshape(6)
    A=ev[0]
    B=ev[1]
    C=ev[2]
    D=ev[3]
    E=ev[4]
    F=ev[5]

    print(f"{A}x^2 + {2*B}xy + {C}y^2 + {2*f0*D}x + {2*f0*E}y + {f0*f0*F} = 0");

    # テスト
    #A=1
    #B=0
    #C=1
    #D=0
    #E=0
    #F=-10
    #x_min=-10
    #x_max=10

    x_min=np.min(x_list)
    x_max=np.max(x_list)
    y_min=np.min(y_list)
    y_max=np.max(y_list)

    # 楕円をプロットするために楕円上の点を集める。
    ex_list=[]
    ey_list=[]

    for x in np.arange(x_min-10, x_max+10, (x_max-x_min)/100):
        det = ((2*E*f0 + 2*B*x) ** 2) - 4*C*(F*f0*f0 + 2*D*f0*x + A*x*x)
        if 0 <= det:
            yP=(-2*E*f0-2*B*x + math.sqrt(det) ) / (2*C)
            yM=(-2*E*f0-2*B*x - math.sqrt(det) ) / (2*C)
            ex_list.append(x)
            ey_list.append(yP)
            ex_list.append(x)
            ey_list.append(yM)

    for y in np.arange(y_min-10, y_max+10, (y_max-y_min)/100):
        det = ((2*D*f0 + 2*B*y) ** 2) - 4*A*(F*f0*f0 + 2*E*f0*y + C*y*y)
        if 0 <= det:
            xP=(-2*D*f0-2*B*y + math.sqrt(det) ) / (2*A)
            xM=(-2*D*f0-2*B*y - math.sqrt(det) ) / (2*A)
            ex_list.append(xP)
            ey_list.append(y)
            ex_list.append(xM)
            ey_list.append(y)

    plt.scatter(ex_list,ey_list, s=1, marker=',')
    plt.scatter(x_list, y_list, s=1, c=w_list, marker=',', cmap='bwr')        
    plt.axis('equal')
    plt.colorbar()
    plt.title("Ransac FNS")
    plt.show()

