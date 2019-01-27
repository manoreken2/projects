// Read Blackmagic Micro Studio Camera 4k RAW AVI v210 YUV 10bit 422 
// Write 12bit Raw DNG file

using System;
using System.IO;
using System.Diagnostics;

namespace BMRawAVIv210ToDng {
    public class Convert {
        private const int IMAGE_W = 3840;
        private const int IMAGE_H = 2160;
        private const int IMAGE_WH = IMAGE_W * IMAGE_H;
        private const int BAYER_W = IMAGE_W + 32;
        private const int BAYER_H = IMAGE_H + 32;
        private const int BAYER_WH = BAYER_W * BAYER_H;

        private const int FOURCC_v210 = 0x30313276;

        public bool Run(string fromAviPath, string toDngPathTemplate) {
            bool result = true;

            string toDirPath = Path.GetDirectoryName(toDngPathTemplate);
            string toFilename = Path.GetFileNameWithoutExtension(toDngPathTemplate);

            using (var ar = new AviReader()) {
                try {
                    if (!ar.Open(fromAviPath)) {
                        Console.WriteLine("AVI read failed : {0}", fromAviPath);
                        return false;
                    }

                    if (ar.mBmpih.biWidth != IMAGE_W
                        || ar.mBmpih.biHeight != IMAGE_H
                        || ar.mBmpih.biCompression != FOURCC_v210) {
                        Console.WriteLine("Not Supported AVI format : {0}", fromAviPath);
                        return false;
                    }

                    for (int i = 0; i < ar.NumImages; ++i) {
                        var img = ar.GetNthImage(i);
                        if (img.Length != IMAGE_W * IMAGE_H * 8 / 3) {
                            Console.WriteLine("Not Supported AVI format : {0}", fromAviPath);
                            return false;
                        }
                        string toPath = string.Format("{0}\\{1}_{2:d5}.dng", toDirPath, toFilename, i);
                        Write(img, toPath);
                        img = null;

                        Console.WriteLine("Write {0}", toPath);
                    }

                    Console.WriteLine("Done. Please fix DNG file using Adobe DNG Converter.");
                } catch (Exception ex) {
                    Console.WriteLine(ex);
                    result = false;
                }
            }

            return result;
        }

        private void Write(byte[] from, string toPath) {
            using (var bw = new BinaryWriter(new FileStream(toPath, FileMode.Create, FileAccess.Write))) {
                DngRW.DngWriter.WriteDngHeader(bw, BAYER_W, BAYER_H, 16, DngRW.DngWriter.CFAPatternType.GRBG);

                var sensorBytes = new byte[BAYER_WH * 2];
                int posT = 0;
                int x = 0;
                int y = 0;
                var p12 = new ushort[8];
                for (int i = 0; ; i += 16) {
                    // Decklink SDK p.217
                    // extract lower 8bit from yuv422 10bit
                    // upper 2bits are discarded
                    uint pF0 = (((uint)from[i + 0]) << 0) + (((uint)from[i + 1]) << 8) + (((uint)from[i + 2]) << 16) + (((uint)from[i + 3]) << 24);
                    uint pF1 = (((uint)from[i + 4]) << 0) + (((uint)from[i + 5]) << 8) + (((uint)from[i + 6]) << 16) + (((uint)from[i + 7]) << 24);
                    uint pF2 = (((uint)from[i + 8]) << 0) + (((uint)from[i + 9]) << 8) + (((uint)from[i + 10]) << 16) + (((uint)from[i + 11]) << 24);
                    uint pF3 = (((uint)from[i + 12]) << 0) + (((uint)from[i + 13]) << 8) + (((uint)from[i + 14]) << 16) + (((uint)from[i + 15]) << 24);

                    byte cr0 = (byte)((pF0 & 0x0ff00000) >> 20);
                    byte y0 = (byte)((pF0 & 0x0003fc00) >> 10);
                    byte cb0 = (byte)((pF0 & 0x000000ff));

                    byte y2 = (byte)((pF1 & 0x0ff00000) >> 20);
                    byte cb2 = (byte)((pF1 & 0x0003fc00) >> 10);
                    byte y1 = (byte)((pF1 & 0x000000ff));

                    byte cb4 = (byte)((pF2 & 0x0ff00000) >> 20);
                    byte y3 = (byte)((pF2 & 0x0003fc00) >> 10);
                    byte cr2 = (byte)((pF2 & 0x000000ff));

                    byte y5 = (byte)((pF3 & 0x0ff00000) >> 20);
                    byte cr4 = (byte)((pF3 & 0x0003fc00) >> 10);
                    byte y4 = (byte)((pF3 & 0x000000ff));

                    // Blackmagic Studio Camera Manual p.59
                    // 12bit RAW sensor data
                    p12[0] = (ushort)((ushort)cb0 | ((y0 & 0xf) << 8));
                    p12[1] = (ushort)(((ushort)cr0 << 4) | (y0 >> 4));
                    p12[2] = (ushort)((ushort)y1 | ((cb2 & 0xf) << 8));
                    p12[3] = (ushort)(((ushort)cb2 >> 4) | (y2 << 4));

                    p12[4] = (ushort)((ushort)cr2 | ((y3 & 0xf) << 8));
                    p12[5] = (ushort)(((ushort)cb4 << 4) | (y3 >> 4));
                    p12[6] = (ushort)((ushort)y4 | ((cr4 & 0xf) << 8));
                    p12[7] = (ushort)(((ushort)cr4 >> 4) | (y5 << 4));

                    for (int j = 0; j < 8; ++j) {
                        // p16 : 16bit value
                        ushort p16 = (ushort)(p12[j] * 16);
                        sensorBytes[posT++] = (byte)(p16 & 0xff);
                        sensorBytes[posT++] = (byte)((p16 >> 8) & 0xff);
                    }

                    if (BAYER_WH * 2 <= posT) {
                        break;
                    }

                    x += 8;
                    if (BAYER_W <= x) {
                        x = 0;
                        ++y;
                    }
                }

                bw.Write(sensorBytes);
            }
        }

    }
}
