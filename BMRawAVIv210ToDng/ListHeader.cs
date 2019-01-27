using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace BMRawAVIv210ToDng {
    class ListHeader {
        public static ListHeader Read(FourCCHeader fcc, BinaryReader br) {
            var r = new ListHeader();

            if (0 != fcc.fourcc.CompareTo("LIST")) {
                throw new ArgumentException("LIST header mismatch");
            }

            r.fcc = fcc.fourcc;
            r.bytes = fcc.bytes;
            r.type = Common.ReadFourCC(br);

            return r;
        }

        public string fcc;
        public uint bytes;
        public string type;
    }
}
