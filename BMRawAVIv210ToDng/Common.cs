using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace BMRawAVIv210ToDng
{
    class Common {
        public static string ReadFourCC(BinaryReader br) {
            var b = br.ReadBytes(4);
            if (b.Length < 4) {
                throw new EndOfStreamException("Common.ReadFourCC");
            }

            var c = new char[4];
            for (int i = 0; i < 4; ++i) {
                c[i] = (char)b[i];
            }

            return new string(c);
        }
    }
}
