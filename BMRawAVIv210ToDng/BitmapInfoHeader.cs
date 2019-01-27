using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace BMRawAVIv210ToDng {
    class BitmapInfoHeader {
        public static BitmapInfoHeader Read(FourCCHeader fcc, BinaryReader br) {
            if (0 != fcc.fourcc.CompareTo("strf")
                    || 40 != fcc.bytes) {
                throw new ArgumentException("BitmapInfoHeader fourcc/size mismatch");
            }

            var r = new BitmapInfoHeader();

            r.biSize = br.ReadUInt32();
            r.biWidth = br.ReadInt32();
            r.biHeight = br.ReadInt32();
            r.biPlanes = br.ReadInt16();
            r.biBitCount = br.ReadInt16();

            r.biCompression = br.ReadUInt32();
            r.biSizeImage = br.ReadUInt32();
            r.biXPelsPerMeter = br.ReadInt32();
            r.biYPelsPerMeter = br.ReadInt32();
            r.biClrUsed = br.ReadUInt32();

            r.biClrImportant = br.ReadUInt32();
            return r;
        }

        public uint biSize;
        public int biWidth;
        public int biHeight;
        public short biPlanes;
        public short biBitCount;

        public uint biCompression;
        public uint biSizeImage;
        public int biXPelsPerMeter;
        public int biYPelsPerMeter;
        public uint biClrUsed;

        public uint biClrImportant;
    }
}
