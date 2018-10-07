﻿using System;
using System.Collections.Generic;
using System.IO;

namespace DngRW {
    public class DngReader {

        private int mNumIFDEntries;

        private bool ReadHeader(BinaryReader br) {
            short byteOrder = br.ReadInt16(); // little endian
            short magic = br.ReadInt16();
            uint offset = br.ReadUInt32();

            Console.WriteLine("ByteOrder={0:X4} Magic={1:X4} Offset={2:X8}", byteOrder, magic, offset);

            if (byteOrder != 0x4949) {
                Console.WriteLine("Error: not little endian!");
                return false;
            }
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

            uint originalTag = br.ReadUInt16();

            int t = (int)originalTag;
            if (ifdt == IFDType.Gps) {
                t += 0x10000;
            }

            int f = br.ReadUInt16();
            int count = br.ReadInt32();

            uint dataOffset = br.ReadUInt32();

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

        private IFDEntry CreateIFDEntry(BinaryReader br, IFDEntry.Tag tag, IFDEntry.FieldType ft, int count) {
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
                        s[i] = br.ReadUInt16();
                    }
                    return new IFDEntry(tag, ft, count, s);
                }
            case IFDEntry.FieldType.SSHORT: {
                    var s = new short[count];
                    for (int i = 0; i < count; ++i) {
                        s[i] = br.ReadInt16();
                    }
                    return new IFDEntry(tag, ft, count, s);
                }
            case IFDEntry.FieldType.FLOAT:
                var f = new float[count];
                for (int i = 0; i < count; ++i) {
                    f[i] = br.ReadSingle();
                }
                return new IFDEntry(tag, ft, count, f);
            case IFDEntry.FieldType.LONG: {
                    var l = new uint[count];
                    for (int i = 0; i < count; ++i) {
                        l[i] = br.ReadUInt32();
                    }
                    return new IFDEntry(tag, ft, count, l);
                }
            case IFDEntry.FieldType.SLONG: {
                    var l = new int[count];
                    for (int i = 0; i < count; ++i) {
                        l[i] = br.ReadInt32();
                    }
                    return new IFDEntry(tag, ft, count, l);
                }
            case IFDEntry.FieldType.DOUBLE:
                var d = new double[count];
                for (int i = 0; i < count; ++i) {
                    d[i] = br.ReadDouble();
                }
                return new IFDEntry(tag, ft, count, d);
            case IFDEntry.FieldType.RATIONAL:
            case IFDEntry.FieldType.SRATIONAL:
                var r = new IFDRational[count];
                for (int i = 0; i < count; ++i) {
                    int numer = br.ReadInt32();
                    int denom = br.ReadInt32();
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
            if (4 < count) {
                count = 4;
                truncated = true;
            }
            for (int i = 0; i < count; ++i) {
                var b = BitConverter.ToDouble(ifde.data, i * 8);
                Console.Write("{0:G4} ", b);
            }
            break;
            case IFDEntry.FieldType.RATIONAL:
            case IFDEntry.FieldType.SRATIONAL:
            if (3 < count) {
                count = 3;
                truncated = true;
            }
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
            case IFDEntry.Tag.CFAPattern:
                {
                    var ba = ifde.GetDataAsByteArray();
                    for (int i = 0; i < ba.Length; ++i) {
                        if (Enum.IsDefined(typeof(IFDEntry.CFAPatternType), (int)ba[i])) {
                            var v = (IFDEntry.CFAPatternType)(int)ba[i];
                            Console.Write("{0} ", v);
                        }
                    }
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
            mNumIFDEntries = br.ReadUInt16();
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
            mNextIFDPos = br.ReadUInt32();
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

            return true;
        }

    }
}
