using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace BMRawAVIv210ToDng {
    class AviMainHeader {
        public static AviMainHeader Read(FourCCHeader fcc, BinaryReader br) {
            var r = new AviMainHeader();

            r.fcc = fcc.fourcc;
            r.cb = fcc.bytes;

            if (0 != r.fcc.CompareTo("avih")
                    || 56 != r.cb) {
                throw new ArgumentException("AviMainHeader fourcc/size mismatch");
            }

            r.dwMicroSecPerFrame = br.ReadUInt32();
            r.dwMaxBytesPersec = br.ReadUInt32();
            r.dwPaddingGranularity = br.ReadUInt32();
            r.dwFlags = br.ReadUInt32();

            r.dwTotalFrames = br.ReadUInt32();
            r.dwInitialFrames = br.ReadUInt32();
            r.dwStreams = br.ReadUInt32();
            r.dwSuggestedBufferSize = br.ReadUInt32();
            r.dwWidth = br.ReadUInt32();

            r.dwHeight = br.ReadUInt32();
            r.dwReserved0 = br.ReadUInt32();
            r.dwReserved1 = br.ReadUInt32();
            r.dwReserved2 = br.ReadUInt32();
            r.dwReserved3 = br.ReadUInt32();
            return r;
        }

        public string fcc;
        public uint cb;
        public uint dwMicroSecPerFrame;
        public uint dwMaxBytesPersec;
        public uint dwPaddingGranularity;
        public uint dwFlags;

        public uint dwTotalFrames;
        public uint dwInitialFrames;
        public uint dwStreams;
        public uint dwSuggestedBufferSize;
        public uint dwWidth;

        public uint dwHeight;
        public uint dwReserved0;
        public uint dwReserved1;
        public uint dwReserved2;
        public uint dwReserved3;
    }
}
