using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DumpAVIHeaders {
    class Program {
        long mVideoCount = 0;
        long mAudioCount = 0;

        struct AviMainHeader {
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
        };

        AviMainHeader mAviMH;

        struct AviStreamHeader {
            public uint fccType;
            public uint fccHandler;
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
        };

        AviStreamHeader mAviSH;

        struct BitmapFileHeader {
            public ushort bfType;
            public uint bfSize;
            public ushort bfReserved1;
            public ushort bfReserved2;
            public uint bfOffBits;
        };

        BitmapFileHeader mBmpfh;

        struct BitmapInfoHeader {
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
        };

        BitmapInfoHeader mBmpih;

        struct WaveFormatEx {
            public ushort wFormatTag;
            public ushort nChannels;
            public uint nSamplesPerSec;
            public uint nAvgBytesPerSec;
            public ushort nBlockAlign;

            public ushort wBitsPerSample;
            public ushort cbSize;
        };

        WaveFormatEx mWavFmt;

        struct IndxChunk {
            public uint cb;
            public ushort wLongsPerEntry;
            public byte bIndexSubType;
            public byte bIndexType;
            public uint nEntriesInUse;

            public uint dwChunkId;
            public uint dwReserved0;
            public uint dwReserved1;
            public uint dwReserved2;
        };

        IndxChunk mIndx;

        static string FourCCToString(uint fourCC) {
            var s = new char[4];
            s[0] = (char)(fourCC & 0xff);
            s[1] = (char)((fourCC >>8) & 0xff);
            s[2] = (char)((fourCC >> 16) & 0xff);
            s[3] = (char)((fourCC >> 24) & 0xff);

            return new string(s);
        }

        static uint StringToFourCC(string s) {
            uint fcc = 
                (uint)((byte)s[0]) +
                (uint)(((byte)s[1]) << 8) +
                (uint)(((byte)s[2]) << 16) +
                (uint)(((byte)s[3]) << 24);

            return fcc;
        }

        uint ReadRiff(BinaryReader br) {
            uint fSize = br.ReadUInt32();
            uint fileType = br.ReadUInt32();

            Console.WriteLine("{0:x12} RIFF size={1:x} {2}",
                br.BaseStream.Position-12, fSize, FourCCToString(fileType));

            return fSize;
        }

        void ReadAviMainHeader(string spc, BinaryReader br) {
            uint cb = br.ReadUInt32();
            if (cb != 56) {
                Console.WriteLine("Unknown AVI Main Header size={0}", cb);
                Environment.Exit(1);
            }

            mAviMH.cb = cb;
            mAviMH.dwMicroSecPerFrame = br.ReadUInt32();
            mAviMH.dwMaxBytesPersec = br.ReadUInt32();
            mAviMH.dwPaddingGranularity = br.ReadUInt32();
            mAviMH.dwFlags = br.ReadUInt32();

            mAviMH.dwTotalFrames = br.ReadUInt32();
            mAviMH.dwInitialFrames = br.ReadUInt32();
            mAviMH.dwStreams = br.ReadUInt32();
            mAviMH.dwSuggestedBufferSize = br.ReadUInt32();
            mAviMH.dwWidth = br.ReadUInt32();

            mAviMH.dwHeight = br.ReadUInt32();

            br.BaseStream.Seek(16, SeekOrigin.Current);

            Console.WriteLine("{0:x12} {1} AviMainHeader {2}x{3} {4:F1}fps, {5}frames, {6}streams",
                br.BaseStream.Position - 64, spc,
                mAviMH.dwWidth, mAviMH.dwHeight, 1.0 / (0.001 * 0.001 * mAviMH.dwMicroSecPerFrame),
                mAviMH.dwTotalFrames, mAviMH.dwStreams);
        }

        void ReadAviStreamHeader(string spc, BinaryReader br) {
            uint cb = br.ReadUInt32();
            if (cb != 56) {
                Console.WriteLine("Unknown AVI Stream Header size={0}", cb);
                Environment.Exit(1);
            }

            mAviSH.fccType = br.ReadUInt32();
            mAviSH.fccHandler = br.ReadUInt32();
            mAviSH.dwFlags = br.ReadUInt32();
            mAviSH.wPriority = br.ReadUInt16();
            mAviSH.wLanguage = br.ReadUInt16();

            mAviSH.dwInitialFrames = br.ReadUInt32();
            mAviSH.dwScale = br.ReadUInt32();
            mAviSH.dwRate = br.ReadUInt32();
            mAviSH.dwStart = br.ReadUInt32();
            mAviSH.dwLength = br.ReadUInt32();

            mAviSH.dwSuggestedBufferSize = br.ReadUInt32();
            mAviSH.dwQuality = br.ReadUInt32();
            mAviSH.dwSampleSize = br.ReadUInt32();

            mAviSH.left = br.ReadInt16();
            mAviSH.top = br.ReadInt16();
            mAviSH.right = br.ReadInt16();
            mAviSH.bottom = br.ReadInt16();

            Console.WriteLine("{0:x12} {1} AviStreamHeader {2}bytes {3} {4}"
                + " dwFlags={5:x8} wPriority={6} wLanguage={7} dwInitialFrames={8} dwScale={9}"
                + " dwRate={10} dwStart={11} dwLength={12} dwSuggestedBufferSize={13}"
                + " dwQuality={14} dwSampleSize={15}",
                br.BaseStream.Position-64, spc,
                cb,
                FourCCToString(mAviSH.fccType), FourCCToString(mAviSH.fccHandler),
                
                mAviSH.dwFlags, mAviSH.wPriority, mAviSH.wLanguage,
                mAviSH.dwInitialFrames, mAviSH.dwScale, mAviSH.dwRate, mAviSH.dwStart,
                mAviSH.dwLength, mAviSH.dwSuggestedBufferSize, mAviSH.dwQuality,
                mAviSH.dwSampleSize);
        }

        void ReadBitmapInfoHeader(string spc, BinaryReader br) {
            uint cb = br.ReadUInt32();
            if (cb != 40) {
                Console.WriteLine("Unknown Bitmap Info header size={0}", cb);
                Environment.Exit(1);
            }

            mBmpih.biSize = br.ReadUInt32();
            mBmpih.biWidth = br.ReadInt32();
            mBmpih.biHeight = br.ReadInt32();
            mBmpih.biPlanes = br.ReadInt16();
            mBmpih.biBitCount = br.ReadInt16();

            mBmpih.biCompression = br.ReadUInt32();
            mBmpih.biSizeImage = br.ReadUInt32();
            mBmpih.biXPelsPerMeter = br.ReadInt32();
            mBmpih.biYPelsPerMeter = br.ReadInt32();
            mBmpih.biClrUsed = br.ReadUInt32();

            mBmpih.biClrImportant = br.ReadUInt32();

            Console.WriteLine("{0:x12} {1} BitmapInfoHeader {2}x{3} {4}bit {5}",
                br.BaseStream.Position-40, spc,
                mBmpih.biWidth, mBmpih.biHeight, mBmpih.biBitCount, FourCCToString(mBmpih.biCompression));

            // BITMAPFILEHEADERも作る。ファイル書き込み用。
            mBmpfh.bfType = 0x4d42;
            mBmpfh.bfSize = 14 + 40 + mBmpih.biSize;
            mBmpfh.bfReserved1 = 0;
            mBmpfh.bfReserved2 = 0;
            mBmpfh.bfOffBits = 14 + 40;
        }

        void WriteBmp(BinaryReader br, byte[] data) {
            string outBmpPath = string.Format("{0}{1:d5}.bmp", mOutBmpPrefix, mVideoCount);
            using (var bw = new BinaryWriter(new FileStream(outBmpPath, FileMode.Create))) {
                bw.Write(mBmpfh.bfType);
                bw.Write(mBmpfh.bfSize);
                bw.Write(mBmpfh.bfReserved1);
                bw.Write(mBmpfh.bfReserved2);
                bw.Write(mBmpfh.bfOffBits);
                
                bw.Write(mBmpih.biSize);
                bw.Write(mBmpih.biWidth);
                bw.Write(mBmpih.biHeight);
                bw.Write(mBmpih.biPlanes);
                bw.Write(mBmpih.biBitCount);

                bw.Write(mBmpih.biCompression);
                bw.Write(mBmpih.biSizeImage);
                bw.Write(mBmpih.biXPelsPerMeter);
                bw.Write(mBmpih.biYPelsPerMeter);
                bw.Write(mBmpih.biClrImportant);

                bw.Write(data);
            }
        }

        void ReadWaveFormatEx(string spc, BinaryReader br) {
            uint cb = br.ReadUInt32();

            long start = br.BaseStream.Position;
            mWavFmt.wFormatTag = br.ReadUInt16();
            mWavFmt.nChannels = br.ReadUInt16();
            mWavFmt.nSamplesPerSec = br.ReadUInt32();
            mWavFmt.nAvgBytesPerSec = br.ReadUInt32();
            mWavFmt.nBlockAlign = br.ReadUInt16();

            mWavFmt.wBitsPerSample = br.ReadUInt16();
            mWavFmt.cbSize = br.ReadUInt16();

            Console.WriteLine("{0:x12} {1} WaveFormatEx {2}bytes {3}Hz {4}bit {5}ch cbSize={6}",
                br.BaseStream.Position-22, spc,
                cb,
                mWavFmt.nSamplesPerSec,
                mWavFmt.wBitsPerSample, mWavFmt.nChannels,
                mWavFmt.cbSize);

            br.BaseStream.Seek(cb - 18, SeekOrigin.Current);
        }
        void ReadUnknownHeader(string spc, string fcc, BinaryReader br) {
            uint bytes = br.ReadUInt32();
            Console.WriteLine("{0:x12} {1} {2} {3}bytes",
                br.BaseStream.Position-8, spc, fcc, bytes);
            br.BaseStream.Seek(bytes, SeekOrigin.Current);
        }
        void ReadJunk(string spc, BinaryReader br) {
            uint bytes = br.ReadUInt32();
            Console.WriteLine("{0:x12} {1} JUNK {2}bytes",
                br.BaseStream.Position - 8, spc, bytes);
            br.BaseStream.Seek(bytes, SeekOrigin.Current);
        }
        void ReadIndx(string spc, BinaryReader br) {
            mIndx.cb = br.ReadUInt32();
            long start = br.BaseStream.Position;

            mIndx.wLongsPerEntry = br.ReadUInt16();
            mIndx.bIndexSubType = br.ReadByte();
            mIndx.bIndexType = br.ReadByte();
            mIndx.nEntriesInUse = br.ReadUInt32();

            mIndx.dwChunkId = br.ReadUInt32();
            mIndx.dwReserved0 = br.ReadUInt32();
            mIndx.dwReserved1 = br.ReadUInt32();
            mIndx.dwReserved2 = br.ReadUInt32();

            Console.WriteLine("{0:x12} {1} AviIndex {2}x{3} entries {4} {5}",
                br.BaseStream.Position - 32, spc, mIndx.wLongsPerEntry,
                mIndx.nEntriesInUse, FourCCToString(mIndx.dwChunkId),
                mIndx.bIndexType==0 ? "IndexOfIndexes" : "IndexOfChunks");

            for (int i=0; i<mIndx.nEntriesInUse; ++i) {
                for (int j = 0; j < mIndx.wLongsPerEntry; ++j) {
                    Console.Write("{0:x} ", br.ReadUInt32());
                }
                Console.WriteLine("");
            }

            br.BaseStream.Seek(start + mIndx.cb, SeekOrigin.Begin);
        }

        void ReadStreamData(string spc, uint cBytes, BinaryReader br) {
            long startPos = br.BaseStream.Position;
            long endPos = startPos + cBytes-4;

            ushort UncompressedVideo = (ushort)(((byte)'d') + (((byte)'b') << 8));
            ushort CompressedVideo = (ushort)(((byte)'d') + (((byte)'c') << 8));
            ushort PaletteChange = (ushort)(((byte)'p') + (((byte)'c') << 8));
            ushort AudioData = (ushort)(((byte)'w') + (((byte)'b') << 8));

            do {
                uint fcc = br.ReadUInt32();
                uint bBytes = br.ReadUInt32();
                if (fcc == StringToFourCC("JUNK")) {
                    br.BaseStream.Seek(bBytes, SeekOrigin.Current);
                    continue;
                }

                var data = br.ReadBytes((int)bBytes);
                if (((fcc>>16) & 0xffff) == UncompressedVideo) {
                    Console.WriteLine("{0:x12}    {1} UncompressedVideo {2}bytes", br.BaseStream.Position-8, FourCCToString(fcc), bBytes);
                    ++mVideoCount;
                    if (0 < mOutBmpPrefix.Length) {
                        WriteBmp(br, data);
                    }
                } else if (((fcc>>16) & 0xffff) == CompressedVideo) {
                    Console.WriteLine("{0:x12}    {1} CompressedVideo {2}bytes", br.BaseStream.Position - 8, FourCCToString(fcc), bBytes);
                    ++mVideoCount;
                } else if (((fcc>>16) & 0xffff) == PaletteChange) {
                    Console.WriteLine("{0:x12}    {1} PaletteChange {2}bytes", br.BaseStream.Position-8, FourCCToString(fcc), bBytes);
                } else if (((fcc>>16) & 0xffff) == AudioData) {
                    Console.WriteLine("{0:x12}    {1} AudioData {2}bytes", br.BaseStream.Position - 8, FourCCToString(fcc), bBytes);
                    ++mAudioCount;
                }

            } while (br.BaseStream.Position < endPos);
            Console.WriteLine("");
        }

        void ReadAviOldIndex(string spc, BinaryReader br) {
            uint bytes = br.ReadUInt32();
            uint count = bytes / 16;
            Console.WriteLine("{0:x12} {1} (optional) AviOldIndex {2} entries",
                br.BaseStream.Position - 8, spc, count);

            br.BaseStream.Seek(bytes, SeekOrigin.Current);
        }


        void ReadHeaders(BinaryReader br, int layer, uint hSize) {
            long currentPos = br.BaseStream.Position;
            long endPos = currentPos + hSize;

            string spc = "";
            for (int i=0; i<layer; ++i) {
                spc = spc + "  ";
            }
            Console.WriteLine("              {0} EndPos={1:x12}", spc, endPos);

            do {
                uint fcc = br.ReadUInt32();
                string fccS = FourCCToString(fcc);
                switch (fccS) {
                    case "LIST":
                        uint bytes = br.ReadUInt32();
                        uint type = br.ReadUInt32();
                        Console.WriteLine("{0:x12} {1} LIST size={2:x} {3}",
                            br.BaseStream.Position-12, spc, bytes, FourCCToString(type));
                        if (type == StringToFourCC("movi")) {
                            ReadStreamData(spc, bytes, br);
                        } else {
                            ReadHeaders(br, layer+1, bytes - 4);
                        }
                        break;
                    case "avih":
                        ReadAviMainHeader(spc, br);
                        break;
                    case "strh":
                        ReadAviStreamHeader(spc, br);
                        break;
                    case "strf":
                        if (mAviSH.fccType == StringToFourCC("vids")) {
                            ReadBitmapInfoHeader(spc, br);
                        } else if (mAviSH.fccType == StringToFourCC("auds")) {
                            ReadWaveFormatEx(spc, br);
                        } else {
                            ReadUnknownHeader(spc, fccS, br);
                        }
                        break;
                    case "idx1":
                        ReadAviOldIndex(spc, br);
                        break;
                    case "indx":
                        ReadIndx(spc, br);
                        break;
                    case "JUNK":
                        ReadJunk(spc, br);
                        break;
                    default:
                        ReadUnknownHeader(spc, fccS, br);
                        break;
                }

                currentPos = br.BaseStream.Position;
            } while (currentPos < endPos);
        }

        string mOutBmpPrefix = "";

        void Run(string inFilename, string outBmpPrefix) {
            mOutBmpPrefix = outBmpPrefix;

            using (var br = new BinaryReader(new FileStream(inFilename, FileMode.Open, FileAccess.Read))) {
                uint RIFF = StringToFourCC("RIFF");

                do {
                    // read RIFF
                    uint h = br.ReadUInt32();
                    if (RIFF != h) {
                        Console.WriteLine("Debug: not RIFF {0}", FourCCToString(h));
                        Environment.Exit(1);
                    }

                    uint fSize = ReadRiff(br);
                    ReadHeaders(br, 0, fSize - 4);
                } while (br.BaseStream.Position < br.BaseStream.Length);
            }

            Console.WriteLine("VideoChunk={0}, AudioChunk={1}", mVideoCount, mAudioCount);
        }

        static void Main(string[] args) {
            if (args.Length < 1 || 2 < args.Length) {
                Console.WriteLine("Usage: DumpAVIHeaders inputFilename [outputBmpFilePrefix]");
                return;
            }

            string inFilename = args[0];
            string outBmpPrefix = "";
            if (2 == args.Length) {
                outBmpPrefix = args[1];
            }

            var self = new Program();
            self.Run(inFilename, outBmpPrefix);
        }
    }
}

