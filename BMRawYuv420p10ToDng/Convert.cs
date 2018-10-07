// Read Blackmagic Micro Studio Camera 4k 12bit raw image data encoded in YUV420p10 file
// Write 12bit Raw DNG file

using System;
using System.IO;

namespace BMRawYuv420p10ToDng {
    public class Convert {
        private const int IMAGE_W = 3840;
        private const int IMAGE_H = 2160;
        private const int DNG_W = IMAGE_W + 32;
        private const int DNG_H = IMAGE_H + 32;

        private ushort[] yImg;
        private ushort[] cbImg;
        private ushort[] crImg;

        public bool Run(string fromYuv420p10Path, string toDngPath) {
            bool result = true;
            try {
                Read(fromYuv420p10Path);
                Write(toDngPath);
            } catch (Exception ex) {
                Console.WriteLine(ex);
                result = false;
            }

            return result;
        }

        private void Read(string fromPath) {
            int WH = IMAGE_W*IMAGE_H;
            int WH2 = IMAGE_W * (IMAGE_H/2);

            // Read YUV420p10 raw image to yImg, cbImg, crImg
            yImg = new ushort[WH];
            cbImg = new ushort[WH2];
            crImg = new ushort[WH2];

            using (var br = new BinaryReader(new FileStream(fromPath, FileMode.Open, FileAccess.Read))) {
                for (int i=0; i<WH; ++i) {
                    yImg[i] = br.ReadUInt16();
                }
                for (int i=0; i<WH2; ++i) {
                    cbImg[i] = br.ReadUInt16();
                }
                for (int i=0; i<WH2; ++i) {
                    crImg[i] = br.ReadUInt16();
                }
            }
        }

        private void Write(string toPath) {
            int WH = IMAGE_W*IMAGE_H;
            
            var sensorRaw = new byte[DNG_W * DNG_H];

            using (var bw = new BinaryWriter(new FileStream(toPath, FileMode.Create, FileAccess.Write))) {
                DngRW.DngWriter.WriteDngHeader(bw, DNG_W, DNG_H, 16, DngRW.DngWriter.CFAPatternType.GRBG);

                int outPx = 0;
                for (int i = 0; i < WH; i += 6) {
                    int yPos = i;
                    int cPos = i / 2;

                    byte y0 = (byte)(yImg[yPos + 0] & 0xff);
                    byte y1 = (byte)(yImg[yPos + 1] & 0xff);
                    byte y2 = (byte)(yImg[yPos + 2] & 0xff);
                    byte y3 = (byte)(yImg[yPos + 3] & 0xff);
                    byte y4 = (byte)(yImg[yPos + 4] & 0xff);
                    byte y5 = (byte)(yImg[yPos + 5] & 0xff);

                    byte cb0 = (byte)(cbImg[cPos + 0] & 0xff);
                    byte cb1 = (byte)(cbImg[cPos + 1] & 0xff);
                    byte cb2 = (byte)(cbImg[cPos + 2] & 0xff);

                    byte cr0 = (byte)(crImg[cPos + 0] & 0xff);
                    byte cr1 = (byte)(crImg[cPos + 1] & 0xff);
                    byte cr2 = (byte)(crImg[cPos + 2] & 0xff);

                    // 12bit RAW sensor data
                    ushort p0 = (ushort)(cb0 | ((y0 & 0xf) << 8));
                    ushort p1 = (ushort)((cr0 << 4) | (y0 >> 4));
                    ushort p2 = (ushort)(y1 | ((cb1 & 0xf) << 8));
                    ushort p3 = (ushort)((cb1 >> 4) | (y2 << 4));

                    ushort p4 = (ushort)(cr1 | ((y3 & 0xf) << 8));
                    ushort p5 = (ushort)((cb2 << 4) | (y3 >> 4));
                    ushort p6 = (ushort)(y4 | ((cr2 & 0xf) << 8));
                    ushort p7 = (ushort)((cr2 >> 4) | (y5 << 4));

                    // 16bit RAW sensor data (zero-padded lower 4bit)
                    ushort p0_16 = (ushort)(p0 << 4);
                    ushort p1_16 = (ushort)(p1 << 4);
                    ushort p2_16 = (ushort)(p2 << 4);
                    ushort p3_16 = (ushort)(p3 << 4);
                    ushort p4_16 = (ushort)(p4 << 4);
                    ushort p5_16 = (ushort)(p5 << 4);
                    ushort p6_16 = (ushort)(p6 << 4);
                    ushort p7_16 = (ushort)(p7 << 4);

                    // Write little endian 16bit data
                    bw.Write(p0_16);
                    bw.Write(p1_16);
                    bw.Write(p2_16);
                    bw.Write(p3_16);

                    bw.Write(p4_16);
                    bw.Write(p5_16);
                    bw.Write(p6_16);
                    bw.Write(p7_16);

                    outPx += 8;
                    if (DNG_W * DNG_H < outPx) {
                        break;
                    }
                }
            }
        }

    }
}
