#include <Eigen/Dense>
#include <iostream>
#include <stdio.h>

void
PrintMat23(const char* name, Eigen::Matrix<double, 2, 3>& t)
{
    printf("%s=\n %e\t%e\t%e\n %e\t%e\t%e\n",
        name,
        t(0, 0), t(0, 1), t(0, 2),
        t(1, 0), t(1, 1), t(1, 2));
}

int main(void)
{
    Eigen::Matrix<double, 2, 3> test23;

    //     y  x
    test23(0, 0) = 1;
    test23(0, 1) = 2;
    test23(0, 2) = 3;
    test23(1, 0) = 4;
    test23(1, 1) = 5;
    test23(1, 2) = 6;

    /*
      →x
    ↓1  2  3
    y 4  5  6

    メモリ上のデータの並び：列優先。
    1 4 2 5 3 6
    https://eigen.tuxfamily.org/dox/GettingStarted.html
    */

    std::cout << "cout=\n" << test23 << std::endl;
    PrintMat23("PrintMat23", test23);

    return 0;
}


