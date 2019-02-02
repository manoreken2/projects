using System;
using System.Collections.Generic;
using System.IO;

namespace DngRW {
    public class DngReader {

        private int mNumIFDEntries;

        private List<IFDEntry> mIFDList = new List<IFDEntry>();

        private readonly int[] mCinemaDNGtagsMandatory = new int[] {
            254,
            256,
            257,
            258,
            259,
            262,
            //273,
            274,
            277,
            //278,
            //279,
            284,
            33421,
            33422,
            50706,
            50708,
            50721,
        };

        private readonly int[] mCinemaDNGtagsNoAllowed = new int[] {
            301,
            318,
            319,
            320,
            347,
            529,
            530,
            531,
            532,
            34675,
            34856,
            37121,
            40961,
            41730,
            41985,
        };

        private List<int> mAppearedTags = new List<int>();

        private enum Endianness {
            BigEndian,
            LittleEndian
        };

        private Endianness mEndian;

        private short ReadInt16(BinaryReader br) {
            if (mEndian == Endianness.LittleEndian) {
                return br.ReadInt16();
            } else {
                var b = br.ReadBytes(2);
                return (short)(b[0] * 256 + b[1]);
            }
        }

        private ushort ReadUInt16(BinaryReader br) {
            if (mEndian == Endianness.LittleEndian) {
                return br.ReadUInt16();
            } else {
                var b = br.ReadBytes(2);
                return (ushort)(b[0] * 256 + b[1]);
            }
        }

        private int ReadInt32(BinaryReader br) {
            if (mEndian == Endianness.LittleEndian) {
                return br.ReadInt32();
            } else {
                var b = br.ReadBytes(4);
                return (int)((b[0] << 24) + (b[1] << 16) + (b[2] << 8) + b[3]);
            }
        }

        private uint ReadUInt32(BinaryReader br) {
            if (mEndian == Endianness.LittleEndian) {
                return br.ReadUInt32();
            } else {
                var b = br.ReadBytes(4);
                return (uint)((b[0] << 24) + (b[1] << 16) + (b[2] << 8) + b[3]);
            }
        }

        private float ReadSingle(BinaryReader br) {
            if (mEndian == Endianness.LittleEndian) {
                return br.ReadSingle();
            } else {
                var b = br.ReadBytes(4);
                var l = new byte[4];
                l[0] = b[3];
                l[1] = b[2];
                l[2] = b[1];
                l[3] = b[0];

                return BitConverter.ToSingle(l, 0);
            }
        }

        private double ReadDouble(BinaryReader br) {
            if (mEndian == Endianness.LittleEndian) {
                return br.ReadDouble();
            } else {
                var b = br.ReadBytes(8);
                var l = new byte[8];
                for (int i = 0; i < 8; ++i) {
                    l[i] = b[7 - i];
                }

                return BitConverter.ToDouble(l, 0);
            }
        }

        private bool ReadHeader(BinaryReader br) {
            short byteOrder = br.ReadInt16();
            if (byteOrder == 0x4949) {
                Console.WriteLine("Byte order == little endian");
                mEndian = Endianness.LittleEndian;
            } else if (byteOrder == 0x4d4d) {
                Console.WriteLine("Byte order == big endian");
                mEndian = Endianness.BigEndian;
            } else {
                Console.WriteLine("Error: byte order err");
                return false;
            }

            short magic = ReadInt16(br);
            uint offset = ReadUInt32(br);

            Console.WriteLine("ByteOrder={0:X4} Magic={1:X4} Offset={2:X8}", byteOrder, magic, offset);

            if (42 != magic) {
                Console.WriteLine("Error: magic number mismatch!");
                return false;
            }

            br.BaseStream.Seek(offset, SeekOrigin.Begin);

            return true;
        }

        enum IFDType {
            General,
            Gps
        };

        private bool ReadIFDEntry(BinaryReader br, IFDType ifdt, out IFDEntry ifdEntry) {
            long pos = br.BaseStream.Position;

            uint originalTag = ReadUInt16(br);

            int t = (int)originalTag;
            if (ifdt == IFDType.Gps) {
                t += 0x10000;
            } else {
                mAppearedTags.Add(t);
            }

            int f = ReadUInt16(br);
            int count = ReadInt32(br);

            uint dataOffset = ReadUInt32(br);

            var tag = IFDEntry.Tag.Unknown;
            if (Enum.IsDefined(typeof(IFDEntry.Tag), t)) {
                tag = (IFDEntry.Tag)t;
            }

            var ft = IFDEntry.FieldType.Unknown;
            if (Enum.IsDefined(typeof(IFDEntry.FieldType), f)) {
                ft = (IFDEntry.FieldType)f;
            }

            int dataBytes = IFDEntry.DataBytes(ft, count);
            if (4 < dataBytes) {
                // dataOffset==データが置いてある位置。

                long cur = br.BaseStream.Position;

                br.BaseStream.Seek(dataOffset, SeekOrigin.Begin);

                ifdEntry = CreateIFDEntry(br, tag, ft, count);
                ifdEntry.dataOffset = dataOffset;

                br.BaseStream.Seek(cur, SeekOrigin.Begin);
            } else {
                // dataPosをもう一度データとして読む。

                long cur = br.BaseStream.Position;

                br.BaseStream.Seek(pos + 8, SeekOrigin.Begin);

                ifdEntry = CreateIFDEntry(br, tag, ft, count);

                br.BaseStream.Seek(cur, SeekOrigin.Begin);
            }

            ifdEntry.originalTag = originalTag;
            ifdEntry.filePosition = pos;

            return true;
        }

        private IFDEntry CreateIFDEntry(BinaryReader br, IFDEntry.Tag tag,
                IFDEntry.FieldType ft, int count) {
            switch (ft) {
                case IFDEntry.FieldType.ASCII:
                case IFDEntry.FieldType.BYTE:
                case IFDEntry.FieldType.SBYTE:
                case IFDEntry.FieldType.UNDEFINED:
                    var b = br.ReadBytes(count);
                    return new IFDEntry(tag, ft, count, b);

                case IFDEntry.FieldType.SHORT: {
                        var s = new ushort[count];
                        for (int i = 0; i < count; ++i) {
                            s[i] = ReadUInt16(br);
                        }
                        return new IFDEntry(tag, ft, count, s);
                    }
                case IFDEntry.FieldType.SSHORT: {
                        var s = new short[count];
                        for (int i = 0; i < count; ++i) {
                            s[i] = ReadInt16(br);
                        }
                        return new IFDEntry(tag, ft, count, s);
                    }
                case IFDEntry.FieldType.FLOAT:
                    var f = new float[count];
                    for (int i = 0; i < count; ++i) {
                        f[i] = ReadSingle(br);
                    }
                    return new IFDEntry(tag, ft, count, f);
                case IFDEntry.FieldType.LONG: {
                        var l = new uint[count];
                        for (int i = 0; i < count; ++i) {
                            l[i] = ReadUInt32(br);
                        }
                        return new IFDEntry(tag, ft, count, l);
                    }
                case IFDEntry.FieldType.SLONG: {
                        var l = new int[count];
                        for (int i = 0; i < count; ++i) {
                            l[i] = ReadInt32(br);
                        }
                        return new IFDEntry(tag, ft, count, l);
                    }
                case IFDEntry.FieldType.DOUBLE:
                    var d = new double[count];
                    for (int i = 0; i < count; ++i) {
                        d[i] = ReadDouble(br);
                    }
                    return new IFDEntry(tag, ft, count, d);
                case IFDEntry.FieldType.RATIONAL:
                case IFDEntry.FieldType.SRATIONAL:
                    var r = new IFDRational[count];
                    for (int i = 0; i < count; ++i) {
                        int numer = ReadInt32(br);
                        int denom = ReadInt32(br);
                        r[i] = new IFDRational(numer, denom);
                    }
                    return new IFDEntry(tag, ft, count, r);
                case IFDEntry.FieldType.Unknown:
                default:
                    return new IFDEntry(tag, ft, count);
            }
        }

        private void ShowData(IFDEntry ifde) {
            var count = ifde.count;

            Console.Write("{0:X8} {1:X4} {2:X4} {3:X8} {4:X8} {5,27} {6,9} ",
                ifde.filePosition, ifde.originalTag, (int)ifde.fieldType, count, ifde.dataOffset, ifde.tag, ifde.fieldType);

            bool truncated = false;
            switch (ifde.fieldType) {
                case IFDEntry.FieldType.ASCII:
                    if (32 < count) {
                        count = 32;
                        truncated = true;
                    }
                    for (int i = 0; i < count; ++i) {
                        var b = ifde.data[i];
                        Console.Write((char)b);
                    }
                    break;

                case IFDEntry.FieldType.BYTE:
                case IFDEntry.FieldType.SBYTE:
                case IFDEntry.FieldType.UNDEFINED:
                    if (12 < count) {
                        count = 12;
                        truncated = true;
                    }
                    for (int i = 0; i < count; ++i) {
                        var b = ifde.data[i];
                        Console.Write("{0:X2} ", b);
                    }
                    break;
                case IFDEntry.FieldType.SHORT:
                case IFDEntry.FieldType.SSHORT:
                    if (8 < count) {
                        count = 8;
                        truncated = true;
                    }
                    for (int i = 0; i < count; ++i) {
                        var b = BitConverter.ToInt16(ifde.data, i * 2);
                        Console.Write("{0:X4} ", b);
                    }
                    break;
                case IFDEntry.FieldType.FLOAT:
                    if (4 < count) {
                        count = 4;
                        truncated = true;
                    }
                    for (int i = 0; i < count; ++i) {
                        var b = BitConverter.ToSingle(ifde.data, i * 4);
                        Console.Write("{0:G4} ", b);
                    }
                    break;
                case IFDEntry.FieldType.LONG:
                case IFDEntry.FieldType.SLONG:
                    if (4 < count) {
                        count = 4;
                        truncated = true;
                    }
                    for (int i = 0; i < count; ++i) {
                        var b = BitConverter.ToInt32(ifde.data, i * 4);
                        Console.Write("{0:X8} ", b);
                    }
                    break;
                case IFDEntry.FieldType.DOUBLE:
                    /*if (4 < count) {
                        count = 4;
                        truncated = true;
                    }*/
                    for (int i = 0; i < count; ++i) {
                        var b = BitConverter.ToDouble(ifde.data, i * 8);
                        Console.Write("{0:G4} ", b);
                    }
                    break;
                case IFDEntry.FieldType.RATIONAL:
                case IFDEntry.FieldType.SRATIONAL:
                    /*if (3 < count) {
                        count = 3;
                        truncated = true;
                    }*/
                    for (int i = 0; i < count; ++i) {
                        var n = BitConverter.ToInt32(ifde.data, i * 8 + 0);
                        var d = BitConverter.ToInt32(ifde.data, i * 8 + 4);
                        Console.Write("{0}/{1} ", n, d);
                    }
                    break;
                case IFDEntry.FieldType.Unknown:
                    break;
            }

            if (truncated) {
                Console.Write("... ");
            }

            // 特殊なタグの表示。
            switch (ifde.tag) {
                case IFDEntry.Tag.PhotometricInterpretation:
                    if (Enum.IsDefined(typeof(IFDEntry.PhotometricInterpretationType), (int)ifde.dataOffset)) {
                        var v = (IFDEntry.PhotometricInterpretationType)ifde.dataOffset;
                        Console.Write("{0}", v);
                    }
                    break;
                case IFDEntry.Tag.Compression:
                    if (Enum.IsDefined(typeof(IFDEntry.CompressionType), (int)ifde.dataOffset)) {
                        var v = (IFDEntry.CompressionType)ifde.dataOffset;
                        Console.Write("{0}", v);
                    }
                    break;
                case IFDEntry.Tag.Orientation:
                    if (Enum.IsDefined(typeof(IFDEntry.OrientationType), (int)ifde.dataOffset)) {
                        var v = (IFDEntry.OrientationType)ifde.dataOffset;
                        Console.Write("{0}", v);
                    }
                    break;
                case IFDEntry.Tag.PlanarConfiguration:
                    if (Enum.IsDefined(typeof(IFDEntry.PlanarConfigurationType), (int)ifde.dataOffset)) {
                        var v = (IFDEntry.PlanarConfigurationType)ifde.dataOffset;
                        Console.Write("{0}", v);
                    }
                    break;
                case IFDEntry.Tag.CFALayout:
                    if (Enum.IsDefined(typeof(IFDEntry.CFALayoutType), (int)ifde.dataOffset)) {
                        var v = (IFDEntry.CFALayoutType)ifde.dataOffset;
                        Console.Write("{0}", v);
                    }
                    break;
                case IFDEntry.Tag.SampleFormat:
                    if (Enum.IsDefined(typeof(IFDEntry.SampleFormatType), (int)ifde.dataOffset)) {
                        var v = (IFDEntry.SampleFormatType)ifde.dataOffset;
                        Console.Write("{0}", v);
                    }
                    break;
                case IFDEntry.Tag.CFARepeatPatternDim:
                    if (ifde.count != 2 || ifde.fieldType != IFDEntry.FieldType.SHORT) {
                        Console.Write("Error: CFARepeatPatternDim malformat");
                    } else {
                        var sa = ifde.GetDataAsUShortArray();
                        Console.Write("Row={0} Cols={1}", sa[0], sa[1]);
                    }
                    break;
                case IFDEntry.Tag.ImageWidth:
                    if (ifde.count != 1) {
                        Console.Write("Error: ImageWidth malformat");
                    } else {
                        Console.Write("Width={0}pixel", ifde.dataOffset);
                    }
                    break;
                case IFDEntry.Tag.BitsPerSample:
                    if (ifde.fieldType != IFDEntry.FieldType.SHORT) {
                        Console.Write("Error: BitsPerSample malformat");
                    } else {
                        Console.Write(", In decimal: ");
                        var sa = ifde.GetDataAsUShortArray();
                        for (int i = 0; i < sa.Length; ++i) {
                            Console.Write("{0} ", sa[i]);
                        }
                    }
                    break;
                case IFDEntry.Tag.ImageLength:
                    if (ifde.count != 1) {
                        Console.Write("Error: ImageLength malformat");
                    } else {
                        Console.Write("Height={0}pixel", ifde.dataOffset);
                    }
                    break;
                case IFDEntry.Tag.CFAPlaneColor:
                    if (ifde.fieldType != IFDEntry.FieldType.BYTE) {
                        Console.Write("Error: CFAPlaneColor malformat");
                    } else {
                        var ba = ifde.GetDataAsByteArray();
                        for (int i = 0; i < ba.Length; ++i) {
                            if (Enum.IsDefined(typeof(IFDEntry.CFAPatternType), (int)ba[i])) {
                                var v = (IFDEntry.CFAPatternType)(int)ba[i];
                                Console.Write("{0} ", v);
                            }
                        }
                    }
                    break;
                case IFDEntry.Tag.CFAPattern: {
                        var ba = ifde.GetDataAsByteArray();
                        for (int i = 0; i < ba.Length; ++i) {
                            if (Enum.IsDefined(typeof(IFDEntry.CFAPatternType), (int)ba[i])) {
                                var v = (IFDEntry.CFAPatternType)(int)ba[i];
                                Console.Write("{0} ", v);
                            }
                        }
                    }
                    break;
                case IFDEntry.Tag.LightSource:
                case IFDEntry.Tag.CalibrationIlluminant1:
                case IFDEntry.Tag.CalibrationIlluminant2:
                    if (Enum.IsDefined(typeof(IFDEntry.LightSourceType), (int)ifde.dataOffset)) {
                        var v = (IFDEntry.LightSourceType)ifde.dataOffset;
                        Console.Write("{0} ", v);
                    } else {
                        Console.Write("reserved ");
                    }
                    break;
                default:
                    break;
            }


            Console.WriteLine("");
        }

        List<long> mSubIFDs = new List<long>();
        long mNextIFDPos;
        long mExifIFDPos;
        long mGpsIFDPos;

        private bool ParseOneIFD(BinaryReader br, IFDType ifdt) {
            mNumIFDEntries = ReadUInt16(br);
            Console.WriteLine("NumIfdEntries={0:X4}", mNumIFDEntries);

            Console.WriteLine("Offs     tag  type count    data/offs");

            for (int i = 0; i < mNumIFDEntries; ++i) {
                IFDEntry ifdEntry;
                if (!ReadIFDEntry(br, ifdt, out ifdEntry)) {
                    // 全て終わり。
                    Console.WriteLine("End Tag reached");
                    return false;
                }

                ShowData(ifdEntry);
                mIFDList.Add(ifdEntry);

                // 特殊なタグの処理。
                switch (ifdEntry.tag) {
                    case IFDEntry.Tag.ExifIFD:
                        if (ifdEntry.fieldType != IFDEntry.FieldType.SHORT
                                && ifdEntry.fieldType != IFDEntry.FieldType.LONG) {
                            throw new FormatException();
                        }
                        mExifIFDPos = ifdEntry.dataOffset;
                        break;
                    case IFDEntry.Tag.GPSInfo:
                        if (ifdEntry.fieldType != IFDEntry.FieldType.SHORT
                                && ifdEntry.fieldType != IFDEntry.FieldType.LONG) {
                            throw new FormatException();
                        }
                        mGpsIFDPos = ifdEntry.dataOffset;
                        break;
                    case IFDEntry.Tag.SubIFDs:
                        switch (ifdEntry.fieldType) {
                            case IFDEntry.FieldType.LONG:
                                var ia = ifdEntry.GetDataAsUIntArray();
                                foreach (var a in ia) {
                                    mSubIFDs.Add(a);
                                }
                                break;
                            case IFDEntry.FieldType.SHORT:
                                var sa = ifdEntry.GetDataAsUShortArray();
                                foreach (var a in sa) {
                                    mSubIFDs.Add(a);
                                }
                                break;
                            default:
                                Console.WriteLine("Error: SubIFDs unexpected tag type {0}", ifdEntry.fieldType);
                                break;
                        }
                        break;
                    default:
                        break;
                }
            }
            // Next IFD offset
            var endPos = br.BaseStream.Position;
            mNextIFDPos = ReadUInt32(br);
            Console.WriteLine("{0:X8} Next IFD Offset=={1:X8}", endPos, mNextIFDPos);
            Console.WriteLine("End.");

            return true;
        }

        public bool ReadHeader(string path) {
            using (var br = new BinaryReader(new FileStream(path, FileMode.Open, FileAccess.Read))) {
                if (!ReadHeader(br)) {
                    return false;
                }

                mNextIFDPos = 0;
                mGpsIFDPos = 0;
                mExifIFDPos = 0;

                do {
                    if (!ParseOneIFD(br, IFDType.General)) {
                        break;
                    }

                    if (mNextIFDPos != 0) {
                        br.BaseStream.Seek(mNextIFDPos, SeekOrigin.Begin);
                    } else {
                        //nextIFDOffset = (uint)mLastPos;
                        //br.BaseStream.Seek(mLastPos, SeekOrigin.Begin);
                    }
                } while (mNextIFDPos != 0);

                if (mExifIFDPos != 0) {
                    Console.WriteLine("Exif IFD -----------");
                    br.BaseStream.Seek(mExifIFDPos, SeekOrigin.Begin);
                    ParseOneIFD(br, IFDType.General);
                }
                if (mGpsIFDPos != 0) {
                    Console.WriteLine("GPS IFD -----------");
                    br.BaseStream.Seek(mGpsIFDPos, SeekOrigin.Begin);
                    ParseOneIFD(br, IFDType.Gps);
                }

                int idx = 0;
                while (0 < mSubIFDs.Count) {
                    Console.WriteLine("Sub IFD {0} -----------", idx);
                    long pos = mSubIFDs[0];
                    mSubIFDs.RemoveAt(0);
                    br.BaseStream.Seek(pos, SeekOrigin.Begin);
                    ParseOneIFD(br, IFDType.General);
                    ++idx;
                }
            }

            var appearedTags = new HashSet<int>();
            foreach (int iTag in mAppearedTags) {
                appearedTags.Add(iTag);
            }

            foreach (int iTag in mCinemaDNGtagsMandatory) {
                if (!appearedTags.Contains(iTag)) {
                    Console.WriteLine("Info: CinemaDNG mandatory tag {0} is missing", (IFDEntry.Tag)iTag);
                }
            }

            foreach (int iTag in mCinemaDNGtagsNoAllowed) {
                if (appearedTags.Contains(iTag)) {
                    Console.WriteLine("Info: CinemaDNG not allowed tag {0} exists", (IFDEntry.Tag)iTag);
                }
            }

            return true;
        }

        private IFDEntry FindIFD(IFDEntry.Tag t) {
            foreach (var item in mIFDList) {
                if (item.tag == t) {
                    return item;
                }
            }

            return null;
        }

        public bool InspectContent(string path) {
            var ifdCompression = FindIFD(IFDEntry.Tag.Compression);
            var ifdWidth = FindIFD(IFDEntry.Tag.ImageWidth);
            var ifdHeight = FindIFD(IFDEntry.Tag.ImageLength);
            var ifdSamplesPerPixel = FindIFD(IFDEntry.Tag.SamplesPerPixel);
            var ifdBitsPerSample = FindIFD(IFDEntry.Tag.BitsPerSample);
            var ifdStripOffset = FindIFD(IFDEntry.Tag.StripOffsets);
            var ifdCFAPattern = FindIFD(IFDEntry.Tag.CFAPattern);

            int compression = ifdCompression.GetDataAsUShortArray()[0];
            int bitsPerSample = ifdBitsPerSample.GetDataAsUShortArray()[0];
            uint width = ifdHeight.GetDataAsUIntArray()[0];
            uint height = ifdHeight.GetDataAsUIntArray()[0];
            int samplesPerPixel = ifdSamplesPerPixel.GetDataAsUShortArray()[0];

            if (compression != (int)IFDEntry.CompressionType.Uncompressed
                    || bitsPerSample <= 8 || 16 < bitsPerSample || samplesPerPixel != 1) {
                return true;
            }

            {
                var cfap = ifdCFAPattern.GetDataAsByteArray();
                if (cfap[0] != (byte)IFDEntry.CFAPatternType.G ||
                    cfap[1] != (byte)IFDEntry.CFAPatternType.R ||
                    cfap[2] != (byte)IFDEntry.CFAPatternType.B ||
                    cfap[3] != (byte)IFDEntry.CFAPatternType.G) {
                    return true;
                }
            }

            ulong [] g0 = new ulong[256];
            ulong [] r = new ulong[256];
            ulong [] b = new ulong[256];
            ulong [] g1 = new ulong[256];

            using (var br = new BinaryReader(new FileStream(path, FileMode.Open, FileAccess.Read))) {
                br.BaseStream.Seek(ifdStripOffset.GetDataAsUIntArray()[0], SeekOrigin.Begin);

                var data = new ushort[width * height];

                int pos = 0;
                for (int h = 0; h < height; ++h) {
                    for (int w = 0; w < width; ++w) {
                        data[pos++] = br.ReadUInt16();
                    }
                }

                for (int h=0; h<height; h+=2) {
                    for (int w=0; w<width; w+=2) {

                        ++g0[data[w + h * width]/256];
                        ++r[data[w+1 + h * width]/256];
                        ++b[data[w + (h+1) * width]/256];
                        ++g1[data[w+1 + (h+1) * width]/256];
                    }
                }
            }

            for (int i=0; i<256; ++i) {
                Console.WriteLine("{0}, {1}, {2}, {3}, {4}", i, g0[i], r[i], b[i], g1[i]);
            }

            return true;
        }
    }
}
