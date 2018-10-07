using System;
using System.Collections.Generic;
using System.IO;

namespace DngRW {
    public class DngWriter {
        private static void WriteUint32At(BinaryWriter bw, long at, UInt32 v) {
            long cur = bw.BaseStream.Position;
            bw.BaseStream.Seek(at, SeekOrigin.Begin);
            bw.Write(v);
            bw.BaseStream.Seek(cur, SeekOrigin.Begin);
        }

        public enum CFAPatternType {
            RGGB,
            BGGR,
            GRBG,
            GBRG,
        }

        public static void WriteDngHeader(BinaryWriter bw, int W, int H, int bitsPerSample, CFAPatternType cfaPtn) {
            int thumbnailW = 256;
            int thumbnailH = 256;

            WriteImageFileHeader(bw);

            /*
            Tiff fields for 8bit CFA:
                NewSubfileType = 0xfe, //< 0
                ImageWidth = 0x100, //< W
                ImageLength = 0x101, //< H
                BitsPerSample = 0x102, //< 8 8 8 (ushort)
                Compression = 0x103, //< 1 (uncompressed)
                PhotometricInterpretation = 0x106, //< 0x8023 (ColorFilterArray)
                Make = 0x10f, //< " "
                Model = 0x110, //< " "
                StripOffsets = 0x111, //< image data file position
                Orientation = 0x112, //< 1
                SamplesPerPixel = 0x115, //< 1
                RowsPerStrip = 0x116, //< H
                StripByteCounts = 0x117, //< W * H
                PlanarConfiguration = 0x11c, //< 1
                Software = 0x131, //< " "
                #DateTime = 0x132, //< " "

            CFA tags
                CFARepeatPatternDim // SHORT 2 2 Rows=2, Cols=2
                CFAPattern          // BYTE 0 1 1 2
                CFAPlaneColor       // BYTE 0 1 2
                CFALayout           // SHORT 1 RectangularOrSquare
                * 
            DNG Required fields:
                DNGVersion          // BYTE 1 4 0 0
                UniqueCameraModel   // " "
            */
            {
                // 最初の2バイトはnum of IFD Entriesなので2足す。
                long startOfIFDEntry = bw.BaseStream.Position + 2;

                // 包括情報＋サムネイル画像の情報。
                var es = new List<IFDEntry>();
                es.Add(new IFDEntry(IFDEntry.Tag.NewSubfileType, IFDEntry.FieldType.LONG, 1, 1));
                es.Add(new IFDEntry(IFDEntry.Tag.ImageWidth, IFDEntry.FieldType.LONG, 1, thumbnailW));
                es.Add(new IFDEntry(IFDEntry.Tag.ImageLength, IFDEntry.FieldType.LONG, 1, thumbnailH));
                es.Add(new IFDEntry(IFDEntry.Tag.BitsPerSample, IFDEntry.FieldType.SHORT, 3, new ushort[] { 8, 8, 8 }));
                es.Add(new IFDEntry(IFDEntry.Tag.Compression, IFDEntry.FieldType.SHORT, 1, (ushort)IFDEntry.CompressionType.Uncompressed));
                es.Add(new IFDEntry(IFDEntry.Tag.PhotometricInterpretation, IFDEntry.FieldType.SHORT, 1, (ushort)IFDEntry.PhotometricInterpretationType.RGB));
                es.Add(new IFDEntry(IFDEntry.Tag.Make, IFDEntry.FieldType.ASCII, 1, new byte[] { 32 }));
                es.Add(new IFDEntry(IFDEntry.Tag.Model, IFDEntry.FieldType.ASCII, 1, new byte[] { 32 }));

                long stripOffsetsPos = startOfIFDEntry + es.Count * 12 + 8;
                es.Add(new IFDEntry(IFDEntry.Tag.StripOffsets, IFDEntry.FieldType.LONG, 1, 0));

                es.Add(new IFDEntry(IFDEntry.Tag.Orientation, IFDEntry.FieldType.SHORT, 1, (ushort)IFDEntry.OrientationType.TopLeft));
                es.Add(new IFDEntry(IFDEntry.Tag.SamplesPerPixel, IFDEntry.FieldType.SHORT, 1, 3));
                es.Add(new IFDEntry(IFDEntry.Tag.RowsPerStrip, IFDEntry.FieldType.LONG, 1, thumbnailH));
                es.Add(new IFDEntry(IFDEntry.Tag.StripByteCounts, IFDEntry.FieldType.LONG, 1, thumbnailW * thumbnailH * 3));

                es.Add(new IFDEntry(IFDEntry.Tag.PlanarConfiguration, IFDEntry.FieldType.SHORT, 1, (ushort)IFDEntry.PlanarConfigurationType.Chunky));
                es.Add(new IFDEntry(IFDEntry.Tag.Software, IFDEntry.FieldType.ASCII, 1, new byte[] { 32 }));

                long subIFDPos = startOfIFDEntry + es.Count * 12 + 8;
                es.Add(new IFDEntry(IFDEntry.Tag.SubIFDs, IFDEntry.FieldType.LONG, 1, 0));

                es.Add(new IFDEntry(IFDEntry.Tag.DNGVersion, IFDEntry.FieldType.BYTE, 4, new byte[] { 1, 4, 0, 0 }));
                es.Add(new IFDEntry(IFDEntry.Tag.UniqueCameraModel, IFDEntry.FieldType.ASCII, 1, new byte[] { 32 }));

                // Num of IFD Entriesが決まったので書き込む。
                bw.Write((short)es.Count);

                // IFDEntryを書き込み。
                foreach (var e in es) {
                    e.WriteEntry(bw);
                }

                // Next IFD offset == 0
                bw.Write((int)0);

                // 4バイトを超えるデータを書き出す。
                foreach (var e in es) {
                    e.WriteData(bw);
                }

                // サムネイル画像の先頭位置が決まったので書き込み。
                WriteUint32At(bw, stripOffsetsPos, (uint)bw.BaseStream.Position);

                // サムネイル画像データを書き込み。
                int v = 0;
                for (int y = 0; y < thumbnailH; ++y) {
                    for (int x = 0; x < thumbnailW; ++x) {
                        bw.Write((byte)(v));
                        bw.Write((byte)(v >> 8));
                        bw.Write((byte)(v >> 16));
                        ++v;
                    }
                }

                // subIFD(フルサイズのCFA RAW画像用)の位置が決まったので書き込む。
                WriteUint32At(bw, subIFDPos, (uint)bw.BaseStream.Position);
            }

            {
                // 最初の2バイトはnum of IFD Entriesなので2足す。
                long startOfIFDEntry = bw.BaseStream.Position + 2;

                // フルサイズのCFA RAW画像の情報。
                var es = new List<IFDEntry>();
                es.Add(new IFDEntry(IFDEntry.Tag.NewSubfileType, IFDEntry.FieldType.LONG, 1, 0));
                es.Add(new IFDEntry(IFDEntry.Tag.ImageWidth, IFDEntry.FieldType.LONG, 1, W));
                es.Add(new IFDEntry(IFDEntry.Tag.ImageLength, IFDEntry.FieldType.LONG, 1, H));
                es.Add(new IFDEntry(IFDEntry.Tag.BitsPerSample, IFDEntry.FieldType.SHORT, 1, new ushort[] { (ushort)bitsPerSample }));
                es.Add(new IFDEntry(IFDEntry.Tag.Compression, IFDEntry.FieldType.SHORT, 1, (ushort)IFDEntry.CompressionType.Uncompressed));
                es.Add(new IFDEntry(IFDEntry.Tag.PhotometricInterpretation, IFDEntry.FieldType.SHORT, 1, (ushort)IFDEntry.PhotometricInterpretationType.ColorFilterArray));

                long stripOffsetsPos = startOfIFDEntry + es.Count * 12 + 8;
                es.Add(new IFDEntry(IFDEntry.Tag.StripOffsets, IFDEntry.FieldType.LONG, 1, 0));

                es.Add(new IFDEntry(IFDEntry.Tag.SamplesPerPixel, IFDEntry.FieldType.SHORT, 1, 1));
                es.Add(new IFDEntry(IFDEntry.Tag.RowsPerStrip, IFDEntry.FieldType.LONG, 1, H));
                es.Add(new IFDEntry(IFDEntry.Tag.StripByteCounts, IFDEntry.FieldType.LONG, 1, W * H * bitsPerSample / 8));
                es.Add(new IFDEntry(IFDEntry.Tag.PlanarConfiguration, IFDEntry.FieldType.SHORT, 1, (ushort)IFDEntry.PlanarConfigurationType.Chunky));
                es.Add(new IFDEntry(IFDEntry.Tag.CFARepeatPatternDim, IFDEntry.FieldType.SHORT, 2, new ushort[] { 2, 2 }));

                {
                    byte[] cfaB = new byte[4];

                    // 0=R, 1=G, 2=B
                    switch (cfaPtn) {
                    case CFAPatternType.BGGR:
                    cfaB = new byte[] { 2, 1, 1, 0 };
                    break;
                    case CFAPatternType.GBRG:
                    cfaB = new byte[] { 1, 2, 0, 1 };
                    break;
                    case CFAPatternType.GRBG:
                    cfaB = new byte[] { 1, 0, 2, 1 };
                    break;
                    case CFAPatternType.RGGB:
                    cfaB = new byte[] { 0, 1, 1, 2 };
                    break;
                    }
                    es.Add(new IFDEntry(IFDEntry.Tag.CFAPattern, IFDEntry.FieldType.BYTE, 4, cfaB));
                }
                es.Add(new IFDEntry(IFDEntry.Tag.CFAPlaneColor, IFDEntry.FieldType.BYTE, 3, new byte[] { 0, 1, 2 }));
                es.Add(new IFDEntry(IFDEntry.Tag.CFALayout, IFDEntry.FieldType.SHORT, 1, (ushort)IFDEntry.CFALayoutType.RectangularOrSquare));

                // Num of IFD Entriesが決まったので書き込む。
                bw.Write((short)es.Count);

                // IFDEntryを書き込み。
                foreach (var e in es) {
                    e.WriteEntry(bw);
                }

                // Next IFD offset == 0
                bw.Write((int)0);

                // 4バイトを超えるデータを書き出す。
                foreach (var e in es) {
                    e.WriteData(bw);
                }

                // CFA RAW画像の先頭位置が決まったので書き込み。
                WriteUint32At(bw, stripOffsetsPos, (uint)bw.BaseStream.Position);

                // この後ろにCFA RAW画像データを書き込む。
            }
        }

        public static void TestCreateDng(int W, int H, string path) {
            int thumbnailW = 256;
            int thumbnailH = 256;

            using (var bw = new BinaryWriter(new FileStream(path, FileMode.Create, FileAccess.Write))) {
                WriteImageFileHeader(bw);

                /*
                Tiff fields for 8bit CFA:
                    NewSubfileType = 0xfe, //< 0
                    ImageWidth = 0x100, //< W
                    ImageLength = 0x101, //< H
                    BitsPerSample = 0x102, //< 8 8 8 (ushort)
                    Compression = 0x103, //< 1 (uncompressed)
                    PhotometricInterpretation = 0x106, //< 0x8023 (ColorFilterArray)
                    Make = 0x10f, //< " "
                    Model = 0x110, //< " "
                    StripOffsets = 0x111, //< image data file position
                    Orientation = 0x112, //< 1
                    SamplesPerPixel = 0x115, //< 1
                    RowsPerStrip = 0x116, //< H
                    StripByteCounts = 0x117, //< W * H
                    PlanarConfiguration = 0x11c, //< 1
                    Software = 0x131, //< " "
                    #DateTime = 0x132, //< " "

                CFA tags
                    CFARepeatPatternDim // SHORT 2 2 Rows=2, Cols=2
                    CFAPattern          // BYTE 0 1 1 2
                    CFAPlaneColor       // BYTE 0 1 2
                    CFALayout           // SHORT 1 RectangularOrSquare
                 * 
                DNG Required fields:
                    DNGVersion          // BYTE 1 4 0 0
                    UniqueCameraModel   // " "
                */
                {
                    // 最初の2バイトはnum of IFD Entriesなので2足す。
                    long startOfIFDEntry = bw.BaseStream.Position + 2;

                    // 包括情報＋サムネイル画像の情報。
                    var es = new List<IFDEntry>();
                    es.Add(new IFDEntry(IFDEntry.Tag.NewSubfileType, IFDEntry.FieldType.LONG, 1, 1));
                    es.Add(new IFDEntry(IFDEntry.Tag.ImageWidth, IFDEntry.FieldType.LONG, 1, thumbnailW));
                    es.Add(new IFDEntry(IFDEntry.Tag.ImageLength, IFDEntry.FieldType.LONG, 1, thumbnailH));
                    es.Add(new IFDEntry(IFDEntry.Tag.BitsPerSample, IFDEntry.FieldType.SHORT, 3, new ushort[] { 8, 8, 8 }));
                    es.Add(new IFDEntry(IFDEntry.Tag.Compression, IFDEntry.FieldType.SHORT, 1, (ushort)IFDEntry.CompressionType.Uncompressed));
                    es.Add(new IFDEntry(IFDEntry.Tag.PhotometricInterpretation, IFDEntry.FieldType.SHORT, 1, (ushort)IFDEntry.PhotometricInterpretationType.RGB));
                    es.Add(new IFDEntry(IFDEntry.Tag.Make, IFDEntry.FieldType.ASCII, 1, new byte[] { 32 }));
                    es.Add(new IFDEntry(IFDEntry.Tag.Model, IFDEntry.FieldType.ASCII, 1, new byte[] { 32 }));

                    long stripOffsetsPos = startOfIFDEntry + es.Count * 12 + 8;
                    es.Add(new IFDEntry(IFDEntry.Tag.StripOffsets, IFDEntry.FieldType.LONG, 1, 0));

                    es.Add(new IFDEntry(IFDEntry.Tag.Orientation, IFDEntry.FieldType.SHORT, 1, (ushort)IFDEntry.OrientationType.TopLeft));
                    es.Add(new IFDEntry(IFDEntry.Tag.SamplesPerPixel, IFDEntry.FieldType.SHORT, 1, 3));
                    es.Add(new IFDEntry(IFDEntry.Tag.RowsPerStrip, IFDEntry.FieldType.LONG, 1, thumbnailH));
                    es.Add(new IFDEntry(IFDEntry.Tag.StripByteCounts, IFDEntry.FieldType.LONG, 1, thumbnailW * thumbnailH * 3));

                    es.Add(new IFDEntry(IFDEntry.Tag.PlanarConfiguration, IFDEntry.FieldType.SHORT, 1, (ushort)IFDEntry.PlanarConfigurationType.Chunky));
                    es.Add(new IFDEntry(IFDEntry.Tag.Software, IFDEntry.FieldType.ASCII, 1, new byte[] { 32 }));

                    long subIFDPos = startOfIFDEntry + es.Count * 12 + 8;
                    es.Add(new IFDEntry(IFDEntry.Tag.SubIFDs, IFDEntry.FieldType.LONG, 1, 0));

                    es.Add(new IFDEntry(IFDEntry.Tag.DNGVersion, IFDEntry.FieldType.BYTE, 4, new byte[] { 1, 4, 0, 0 }));
                    es.Add(new IFDEntry(IFDEntry.Tag.UniqueCameraModel, IFDEntry.FieldType.ASCII, 1, new byte[] { 32 }));

                    // Num of IFD Entriesが決まったので書き込む。
                    bw.Write((short)es.Count);

                    // IFDEntryを書き込み。
                    foreach (var e in es) {
                        e.WriteEntry(bw);
                    }

                    // Next IFD offset == 0
                    bw.Write((int)0);

                    // 4バイトを超えるデータを書き出す。
                    foreach (var e in es) {
                        e.WriteData(bw);
                    }

                    // サムネイル画像の先頭位置が決まったので書き込み。
                    WriteUint32At(bw, stripOffsetsPos, (uint)bw.BaseStream.Position);

                    // サムネイル画像データを書き込み。
                    int v = 0;
                    for (int y = 0; y < thumbnailH; ++y) {
                        for (int x = 0; x < thumbnailW; ++x) {
                            bw.Write((byte)(v));
                            bw.Write((byte)(v>>8));
                            bw.Write((byte)(v>>16));
                            ++v;
                        }
                    }

                    // subIFD(フルサイズのCFA RAW画像用)の位置が決まったので書き込む。
                    WriteUint32At(bw, subIFDPos, (uint)bw.BaseStream.Position);
                }

                {
                    // 最初の2バイトはnum of IFD Entriesなので2足す。
                    long startOfIFDEntry = bw.BaseStream.Position + 2;

                    // フルサイズのCFA RAW画像の情報。
                    var es = new List<IFDEntry>();
                    es.Add(new IFDEntry(IFDEntry.Tag.NewSubfileType, IFDEntry.FieldType.LONG, 1, 0));
                    es.Add(new IFDEntry(IFDEntry.Tag.ImageWidth, IFDEntry.FieldType.LONG, 1, W));
                    es.Add(new IFDEntry(IFDEntry.Tag.ImageLength, IFDEntry.FieldType.LONG, 1, H));
                    es.Add(new IFDEntry(IFDEntry.Tag.BitsPerSample, IFDEntry.FieldType.SHORT, 1, new ushort[] { 8 }));
                    es.Add(new IFDEntry(IFDEntry.Tag.Compression, IFDEntry.FieldType.SHORT, 1, (ushort)IFDEntry.CompressionType.Uncompressed));
                    es.Add(new IFDEntry(IFDEntry.Tag.PhotometricInterpretation, IFDEntry.FieldType.SHORT, 1, (ushort)IFDEntry.PhotometricInterpretationType.ColorFilterArray));

                    long stripOffsetsPos = startOfIFDEntry + es.Count * 12 + 8;
                    es.Add(new IFDEntry(IFDEntry.Tag.StripOffsets, IFDEntry.FieldType.LONG, 1, 0));

                    es.Add(new IFDEntry(IFDEntry.Tag.SamplesPerPixel, IFDEntry.FieldType.SHORT, 1, 1));
                    es.Add(new IFDEntry(IFDEntry.Tag.RowsPerStrip, IFDEntry.FieldType.LONG, 1, H));
                    es.Add(new IFDEntry(IFDEntry.Tag.StripByteCounts, IFDEntry.FieldType.LONG, 1, W * H));
                    es.Add(new IFDEntry(IFDEntry.Tag.PlanarConfiguration, IFDEntry.FieldType.SHORT, 1, (ushort)IFDEntry.PlanarConfigurationType.Chunky));
                    es.Add(new IFDEntry(IFDEntry.Tag.CFARepeatPatternDim, IFDEntry.FieldType.SHORT, 2, new ushort[] { 2, 2 }));
                    es.Add(new IFDEntry(IFDEntry.Tag.CFAPattern, IFDEntry.FieldType.BYTE, 4, new byte[] {
                                            0, 1,
                                            1, 2 })); // 0=R, 1=G, 2=B
                    es.Add(new IFDEntry(IFDEntry.Tag.CFAPlaneColor, IFDEntry.FieldType.BYTE, 3, new byte[] { 0, 1, 2 }));
                    es.Add(new IFDEntry(IFDEntry.Tag.CFALayout, IFDEntry.FieldType.SHORT, 1, (ushort)IFDEntry.CFALayoutType.RectangularOrSquare));

                    // Num of IFD Entriesが決まったので書き込む。
                    bw.Write((short)es.Count);

                    // IFDEntryを書き込み。
                    foreach (var e in es) {
                        e.WriteEntry(bw);
                    }

                    // Next IFD offset == 0
                    bw.Write((int)0);

                    // 4バイトを超えるデータを書き出す。
                    foreach (var e in es) {
                        e.WriteData(bw);
                    }

                    // CFA RAW画像の先頭位置が決まったので書き込み。
                    WriteUint32At(bw, stripOffsetsPos, (uint)bw.BaseStream.Position);
                    
                    // 画像データを書き込み。
                    int v = 0;
                    for (int y = 0; y < H; ++y) {
                        for (int x = 0; x < W; ++x) {
                            if (0 == ((x+y) & 1)) {
                                bw.Write((byte)255);
                            } else {
                                bw.Write((byte)(v));
                            }
                            ++v;
                        }
                    }
                }
            }
        }

        public static void TestCreateTiff(int W, int H, string path) {
            using (var bw = new BinaryWriter(new FileStream(path, FileMode.Create, FileAccess.Write))) {
                WriteImageFileHeader(bw);

                // TBD number of directory entry 
                long numIFDEntryOffs = bw.BaseStream.Position;
                bw.Write((short)0);

                /*
                TIFF RGB image required fields:
                ImageWidth                SHORT or LONG
                ImageLength               SHORT or LONG
                BitsPerSample             SHORT 8,8,8
                Compression               SHORT 1 or 32773
                PhotometricInterpretation SHORT 2
                StripOffsets              SHORT or LONG
                SamplesPerPixel           SHORT 3 or more
                RowsPerStrip              SHORT or LONG
                StripByteCounts           LONG or SHORT
                XResolution               RATIONAL
                YResolution               RATIONAL
                ResolutionUnit            SHORT 1, 2 or 3
                */
                
                long startOfIFDEntry = bw.BaseStream.Position;

                var es = new List<IFDEntry>();
                es.Add(new IFDEntry(IFDEntry.Tag.NewSubfileType, IFDEntry.FieldType.LONG, 1, 0));
                es.Add(new IFDEntry(IFDEntry.Tag.ImageWidth, IFDEntry.FieldType.LONG, 1, W));
                es.Add(new IFDEntry(IFDEntry.Tag.ImageLength, IFDEntry.FieldType.LONG, 1, H));
                es.Add(new IFDEntry(IFDEntry.Tag.Orientation, IFDEntry.FieldType.SHORT, 1, (short)IFDEntry.OrientationType.TopLeft));
                es.Add(new IFDEntry(IFDEntry.Tag.BitsPerSample, IFDEntry.FieldType.SHORT, 3, new short[] { 8, 8, 8 }));
                es.Add(new IFDEntry(IFDEntry.Tag.SampleFormat, IFDEntry.FieldType.SHORT, 1, (short)IFDEntry.SampleFormatType.Uint));
                es.Add(new IFDEntry(IFDEntry.Tag.Compression, IFDEntry.FieldType.SHORT, 1, (short)IFDEntry.CompressionType.Uncompressed));
                es.Add(new IFDEntry(IFDEntry.Tag.PhotometricInterpretation, IFDEntry.FieldType.SHORT, 1, (short)IFDEntry.PhotometricInterpretationType.RGB));

                long stripOffsetsPos = startOfIFDEntry + es.Count * 12 + 8;
                es.Add(new IFDEntry(IFDEntry.Tag.StripOffsets, IFDEntry.FieldType.LONG, 1, 0));
                
                es.Add(new IFDEntry(IFDEntry.Tag.SamplesPerPixel, IFDEntry.FieldType.SHORT, 1, 3));
                es.Add(new IFDEntry(IFDEntry.Tag.RowsPerStrip, IFDEntry.FieldType.LONG, 1, H));
                es.Add(new IFDEntry(IFDEntry.Tag.StripByteCounts, IFDEntry.FieldType.LONG, 1, W*H*3));
                es.Add(new IFDEntry(IFDEntry.Tag.PlanarConfiguration, IFDEntry.FieldType.SHORT, 1, (short)IFDEntry.PlanarConfigurationType.Chunky));
                es.Add(new IFDEntry(IFDEntry.Tag.XResolution, IFDEntry.FieldType.RATIONAL, 1, new IFDRational(300,1)));
                es.Add(new IFDEntry(IFDEntry.Tag.YResolution, IFDEntry.FieldType.RATIONAL, 1, new IFDRational(300, 1)));
                es.Add(new IFDEntry(IFDEntry.Tag.ResolutionUnit, IFDEntry.FieldType.SHORT, 1, (short)IFDEntry.ResolutionUnitType.Inch));

                // IFDEntryの数が決まったので書き込む。
                long cur = bw.BaseStream.Position;
                bw.BaseStream.Seek(numIFDEntryOffs, SeekOrigin.Begin);
                bw.Write((short)es.Count);
                bw.BaseStream.Seek(cur, SeekOrigin.Begin);

                // IFDEntryを書き込み。
                foreach (var e in es) {
                    e.WriteEntry(bw);
                }

                // Next IFD offset == 0
                bw.Write((int)0);

                // 4バイトを超えるデータを書き出す。
                foreach (var e in es) {
                    e.WriteData(bw);
                }

                // 画像の先頭位置が決まったので書き込み。
                cur = bw.BaseStream.Position;
                bw.BaseStream.Seek(stripOffsetsPos, SeekOrigin.Begin);
                bw.Write((int)cur);
                bw.BaseStream.Seek(cur, SeekOrigin.Begin);

                // 画像データを書き込み。
                int v = 0;
                for (int y=0; y<H;++y) {
                    for (int x=0; x<W;++x) {
                        bw.Write((byte)(v));
                        bw.Write((byte)(v>>8));
                        bw.Write((byte)(v>>16));
                        ++v;
                    }
                }
            }
        }

        private static void WriteImageFileHeader(BinaryWriter bw) {
            ushort byteOrder = 0x4949; // little endian
            ushort magic = 0x2a;
            uint offset = 0x8;

            bw.Write(byteOrder);
            bw.Write(magic);
            bw.Write(offset);
        }
    }
}
