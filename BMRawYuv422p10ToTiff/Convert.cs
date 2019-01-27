// Read Blackmagic Micro Studio Camera 4k 12bit raw image data encoded in YUV422p10 file
// Write 12bit Raw DNG file

using System;
using System.IO;
using System.Diagnostics;

namespace BMRawYuv422p10ToTiff {
    public class Convert {
        private const int IMAGE_W = 3840;
        private const int IMAGE_H = 2160;
        private const int OUT_W = IMAGE_W + 32;
        private const int OUT_H = IMAGE_H + 32;

        private ushort[] yImg;
        private ushort[] cbImg;
        private ushort[] crImg;

        public bool Run(string fromYuv422p10Path, string toPathTemplate) {
            bool result = true;

            string toDirPath = Path.GetDirectoryName(toPathTemplate);
            string toFilename = Path.GetFileNameWithoutExtension(toPathTemplate);

            try {
                for (int i = 0; ; ++i) {
                    if (!Read(fromYuv422p10Path, i)) {
                        break;
                    }

                    string toPath = string.Format("{0}\\{1}_{2:d5}.tif", toDirPath, toFilename, i);
                    Write(toPath);
                }
            } catch (Exception ex) {
                Console.WriteLine(ex);
                result = false;
            }


            return result;
        }

        /// <returns>true: success. false: reach EOF.</returns>
        private bool Read(string fromPath, int imageIdx) {
            int WH = IMAGE_W*IMAGE_H;
            int WH2 = IMAGE_W * (IMAGE_H/2);

            // Read YUV420p10 raw image to yImg, cbImg, crImg
            yImg = new ushort[WH];
            cbImg = new ushort[WH2];
            crImg = new ushort[WH2];

            using (var br = new BinaryReader(new FileStream(fromPath, FileMode.Open, FileAccess.Read))) {
                long skipOffset = imageIdx * (WH + WH2 + WH2) * 2 /* sizeof short */;
                br.BaseStream.Seek(skipOffset, SeekOrigin.Begin);

                byte[] b;
                b = br.ReadBytes(WH * 2);
                if (b.Length < WH * 2) {
                    Console.WriteLine("Reach EOF.");
                    return false;
                }
                for (int i = 0; i < WH; ++i)
                {
                    yImg[i] = BitConverter.ToUInt16(b, i*2);
                }
                b = br.ReadBytes(WH2 * 2);
                for (int i = 0; i < WH2; ++i)
                {
                    cbImg[i] = BitConverter.ToUInt16(b, i * 2);
                }
                b = br.ReadBytes(WH2 * 2);
                for (int i = 0; i < WH2; ++i)
                {
                    crImg[i] = BitConverter.ToUInt16(b, i * 2);
                }
            }

            return true;
        }

        private void SetRGB(ref ushort[] buf, int x, int y, ushort r, ushort g, ushort b) {
            buf[(y * OUT_W + x) * 3 + 0] = r;
            buf[(y * OUT_W + x) * 3 + 1] = g;
            buf[(y * OUT_W + x) * 3 + 2] = b;
        }

        private void Write(string toPath) {
            int WH = IMAGE_W*IMAGE_H;
            
            using (var bw = new BinaryWriter(new FileStream(toPath, FileMode.Create, FileAccess.Write))) {
                DngRW.DngWriter.WriteTiffHeader(bw, OUT_W, OUT_H, 16);

                ushort[] p16 = new ushort[8];
                var sensorRaw = new ushort[OUT_W * OUT_H];
                int count = 0;
                
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
                    ushort p0_12 = (ushort)(cb0 | ((y0 & 0xf) << 8));
                    ushort p1_12 = (ushort)((cr0 << 4) | (y0 >> 4));
                    ushort p2_12 = (ushort)(y1 | ((cb1 & 0xf) << 8));
                    ushort p3_12 = (ushort)((cb1 >> 4) | (y2 << 4));

                    ushort p4_12 = (ushort)(cr1 | ((y3 & 0xf) << 8));
                    ushort p5_12 = (ushort)((cb2 << 4) | (y3 >> 4));
                    ushort p6_12 = (ushort)(y4 | ((cr2 & 0xf) << 8));
                    ushort p7_12 = (ushort)((cr2 >> 4) | (y5 << 4));

                    // 16bit RAW sensor data (zero-padded lower 4bit)
                    p16[0] = (ushort)(p0_12 << 4);
                    p16[1] = (ushort)(p1_12 << 4);
                    p16[2] = (ushort)(p2_12 << 4);
                    p16[3] = (ushort)(p3_12 << 4);

                    p16[4] = (ushort)(p4_12 << 4);
                    p16[5] = (ushort)(p5_12 << 4);
                    p16[6] = (ushort)(p6_12 << 4);
                    p16[7] = (ushort)(p7_12 << 4);

                    for (int j = 0; j < 8; ++j) {
                        sensorRaw[count++] = p16[j];
                    }

                    if (sensorRaw.Length <= count) {
                        break;
                    }
                }

                var buff = new ushort[OUT_W * OUT_H * 3];
                for (int y = 0; y < OUT_H; y+=2) {
                    for (int x = 0; x < OUT_W; x+=2) {
                        // G R
                        // B G
                        ushort g0 = sensorRaw[(y + 0) * OUT_W + x + 0];
                        ushort r = sensorRaw[(y + 0) * OUT_W + x + 1];
                        ushort b = sensorRaw[(y + 1) * OUT_W + x + 0];
                        ushort g1 = sensorRaw[(y + 1) * OUT_W + x + 1];

                        SetRGB(ref buff, x, y, r, g0, b);
                        SetRGB(ref buff, x+1, y, r, g0, b);
                        SetRGB(ref buff, x, y+1, r, g1, b);
                        SetRGB(ref buff, x+1, y+1, r, g1, b);
                    }
                }

                for (int i = 0; i < OUT_W * OUT_H * 3; ++i) {
                    bw.Write(buff[i]);
                }
            }
        }

    }
}
