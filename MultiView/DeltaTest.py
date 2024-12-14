import numpy as np
import math
import matplotlib.pyplot as plt

def sinc_x(x : np.float64):
    if np.abs(x) < 1e-16:
        return 1.0
    else:
        return math.sin(x) / x

def sinc_pi_x(x):
    return sinc_x(x * math.pi)    

def sinc_interp(y_list, ratio):
    xa_list = []
    ya_list = []
    c_list  = []

    x_step = 1 * math.pi

    for xi in range(len(y_list)*ratio):
        c = 0
        if xi % ratio == 0:
            c = 1
        c_list.append(c)

        xa = xi / ratio
        xa_list.append(xa)

        ya = 0.0
        for i in range(len(y_list)):
            x = (i - xi / ratio) * x_step
            y = y_list[i]
            ya += y * sinc_x(x)
        ya_list.append(ya)

    return xa_list, ya_list, c_list

def test1():
    INTERP = 2
    N = 10

    y_list = N * [0.0]
    y_list[4] = 1.0
    y_list[5] = 1.0

    xa_list, ya_list = sinc_interp(y_list, INTERP)
    plt.scatter(xa_list, ya_list)
    plt.show()

def build_symmetry(y_list):
    r_list = []

    for y in y_list:
        r_list.append(y)
        r_list.insert(0, y)

    return r_list

def absmax(y_list):
    y_list_NP = np.asarray(y_list)
    return np.max([np.abs(y_list_NP.max()),  np.abs(y_list_NP.min())])

def normalize(y_list):
    m = absmax(y_list)
    r_list = []
    for y in y_list:
        r_list.append(y/m)

    return r_list

def run(n, p):
    x_list=[]
    y_list=[]

    xOdd=[]
    yOdd=[]

    xEven=[]
    yEven=[]

    c=0

    x_step = math.pi/p

    s=1000*4
    x_start = -s * x_step
    x_end   =  s * x_step

    for x in np.arange(x_start, x_end, x_step):
        c=c+1

        x_list.append(x)

        y = sinc_x(x)

        y_list.append(y)

        if c % 2 == 0:
            xEven.append(x)
            yEven.append(y)
        else:
            xOdd.append(x)
            yOdd.append(y)


    #plt.scatter(x_list, y_list)
    #plt.show()

    #plt.scatter(xEven, yEven)
    #plt.scatter(xOdd, yOdd)

    #yOdd  = normalize(yOdd)
    yEven = normalize(yEven)

    #plt.scatter(xEven, yEven, marker='.')
    #plt.scatter(xOdd, yOdd, marker='.')

    xIntp_list, yIntp_list, color_list = sinc_interp(yEven, 2)
    #plt.scatter(xIntp_list, yIntp_list, c=color_list, marker='.')
    #plt.title(f"n={n} p={p}")
    plt.show()

    yIntpAbsMax = absmax(yIntp_list)

    return yIntpAbsMax

def sweep():
    p_list=[]
    r_list=[]
    pMax = 0
    rMax = 0
    for p in np.arange(start=0.079, stop=0.081, step=0.00001):
        p_list.append(p)
        
        r = run(n=100, p=p)
        r_list.append(r)

        if rMax < r:
            rMax = r
            pMax = p
        print(f"{p} {r}")
    print(f"pMax={pMax} rMax={rMax}")
    plt.scatter(p_list, r_list, marker='.')
    plt.show()

#sweep()

#test1()

#r = run(n=100, p=2.0)
#r = run(n=100, p=0.08)

#print(r)

#for x in np.arange(0, 2, 0.25):
#    y = sinc_pi_x(x) 
#    print(f"x={x} y={y}")

print(1 / sinc_pi_x(0.5))
