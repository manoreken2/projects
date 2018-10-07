using System;

namespace DngRW {
    public class IFDRational {
        public int numer;
        public int denom;
        public IFDRational(int n, int d) {
            numer = n;
            denom = d;

            if (denom == 0) {
                throw new DivideByZeroException();
            }
            if (denom < 0) {
                throw new ArgumentOutOfRangeException("d");
            }
        }
    }
}
