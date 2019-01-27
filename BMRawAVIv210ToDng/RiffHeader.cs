using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace BMRawAVIv210ToDng {
    class RiffHeader {
        public static RiffHeader Read(BinaryReader br) {
            var r = new RiffHeader();
            r.riff = Common.ReadFourCC(br);
            r.bytes = br.ReadUInt32();
            r.type = Common.ReadFourCC(br);
            return r;
        }

        public string riff;
        public uint bytes;
        public string type;
    }
}
