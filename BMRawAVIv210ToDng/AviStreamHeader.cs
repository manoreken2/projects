using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace BMRawAVIv210ToDng {
    class AviStreamHeader {
        public static AviStreamHeader Read(FourCCHeader fcc, BinaryReader br) {
            var r = new AviStreamHeader();
            r.fcc = fcc.fourcc;
            r.cb = fcc.bytes;

            if (0 != r.fcc.CompareTo("strh")
                    || 56 != r.cb) {
                throw new ArgumentException("AviStreamHeader fourcc/size mismatch");
            }

            r.fccType = Common.ReadFourCC(br);
            r.fccHandler = Common.ReadFourCC(br);
            r.dwFlags = br.ReadUInt32();
            r.wPriority = br.ReadUInt16();
            r.wLanguage = br.ReadUInt16();

            r.dwInitialFrames = br.ReadUInt32();
            r.dwScale = br.ReadUInt32();
            r.dwRate = br.ReadUInt32();
            r.dwStart = br.ReadUInt32();
            r.dwLength = br.ReadUInt32();

            r.dwSuggestedBufferSize = br.ReadUInt32();
            r.dwQuality = br.ReadUInt32();
            r.dwSampleSize = br.ReadUInt32();

            r.left = br.ReadInt16();
            r.top = br.ReadInt16();

            r.right = br.ReadInt16();
            r.bottom = br.ReadInt16();

            return r;
        }

        public string fcc; //< "strh"
        public uint cb;

        public string fccType;
        public string fccHandler;
        public uint dwFlags;
        public ushort wPriority;
        public ushort wLanguage;

        public uint dwInitialFrames;
        public uint dwScale;
        public uint dwRate;
        public uint dwStart;
        public uint dwLength;

        public uint dwSuggestedBufferSize;
        public uint dwQuality;
        public uint dwSampleSize;
        public short left;
        public short top;

        public short right;
        public short bottom;
    }
}
