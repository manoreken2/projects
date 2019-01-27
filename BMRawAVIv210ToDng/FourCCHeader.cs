using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace BMRawAVIv210ToDng {
    class FourCCHeader {
        public static FourCCHeader Read(BinaryReader br) {
            var fh = new FourCCHeader();
            fh.fourcc = Common.ReadFourCC(br);
            fh.bytes = br.ReadUInt32();
            return fh;
        }

        public string fourcc;
        public uint bytes;
    }
}
