using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace BMRawAVIv210ToDng
{
    class AviReader : IDisposable {
        public class PositionAndSize {
            public long position;
            public uint bytes;
        };

        /* RIFF "AVI "
             LIST "hdrl"
               avih AviMainHeader
               LIST "strl"
                 strh AviStreamHeader
                 strf BitmapInfoHeader
             LIST "movi"
               00db
               00db
               ...
           RIFF "AVIX"
             LIST "movi"
               00db
               00db
               ...
        */

        public bool Open(string path) {
            mImagePos.Clear();

            mBr = new BinaryReader(new FileStream(path, FileMode.Open, FileAccess.Read));

            var riffHeader = RiffHeader.Read(mBr);
            if (0 != riffHeader.riff.CompareTo("RIFF")
                    || 0!= riffHeader.type.CompareTo("AVI ")) {
                Console.WriteLine("E: this is not AVI");
                return false;
            }

            try {
                do {
                    var fcc = FourCCHeader.Read(mBr);
                    Console.WriteLine("{0}", fcc.fourcc);
                    switch (fcc.fourcc) {
                        case "RIFF":
                            var riffType = Common.ReadFourCC(mBr);
                            if (0 != riffType.CompareTo("AVIX")) {
                                Console.WriteLine("D: Unknown Riff {0}", riffType);
                            }
                            break;
                        case "LIST":
                            var lst = ListHeader.Read(fcc, mBr);
                            break;
                        case "avih":
                            mAviMainHeader = AviMainHeader.Read(fcc, mBr);
                            break;
                        case "strh":
                            mAviStreamHeader = AviStreamHeader.Read(fcc, mBr);
                            break;
                        case "strf":
                            if (0 == mAviStreamHeader.fccType.CompareTo("vids")) {
                                mBmpih = BitmapInfoHeader.Read(fcc, mBr);
                            } else {
                                SkipUnknownHeader(fcc);
                            }
                            break;
                        case "00db":
                        case "00dc":
                            ReadOneImage(fcc);
                            break;
                        default:
                            SkipUnknownHeader(fcc);
                            break;
                    }
                } while (true);
            } catch (EndOfStreamException) {
                // OK
            }

            Console.WriteLine("Total {0} images", mImagePos.Count);

            if (mImagePos.Count == 0) {
                Close();
                return false;
            }

            return true;
        }

        public void Close() {
            if (mBr != null) { 
                mBr.Close();
                mBr = null;
            }
        }

        public void ReadOneImage(FourCCHeader fcc) {
            PositionAndSize ps = new PositionAndSize();
            ps.bytes = fcc.bytes;
            ps.position = mBr.BaseStream.Position;
            mImagePos.Add(ps);

            mBr.BaseStream.Seek(fcc.bytes, SeekOrigin.Current);
        }

        public void SkipUnknownHeader(FourCCHeader fcc) {
            mBr.BaseStream.Seek(fcc.bytes, SeekOrigin.Current);
        }

        public int NumImages {
            get {
                return mImagePos.Count;
            }
        }

        public byte[] GetNthImage(int nth) {
            var ps = mImagePos[nth];
            mBr.BaseStream.Seek(ps.position, SeekOrigin.Begin);
            return mBr.ReadBytes((int)ps.bytes);
        }

        public AviMainHeader mAviMainHeader;
        public AviStreamHeader mAviStreamHeader;
        public BitmapInfoHeader mBmpih;
        public BinaryReader mBr = null;

        public List<PositionAndSize> mImagePos = new List<PositionAndSize>();

        #region IDisposable Support
        private bool disposedValue = false; // To detect redundant calls

        protected virtual void Dispose(bool disposing) {
            if (!disposedValue) {
                if (disposing) {
                    // TODO: dispose managed state (managed objects).
                    Close();
                }

                // TODO: free unmanaged resources (unmanaged objects) and override a finalizer below.
                // TODO: set large fields to null.

                disposedValue = true;
            }
        }

        // TODO: override a finalizer only if Dispose(bool disposing) above has code to free unmanaged resources.
        // ~AviReader() {
        //   // Do not change this code. Put cleanup code in Dispose(bool disposing) above.
        //   Dispose(false);
        // }

        // This code added to correctly implement the disposable pattern.
        public void Dispose() {
            // Do not change this code. Put cleanup code in Dispose(bool disposing) above.
            Dispose(true);
            // TODO: uncomment the following line if the finalizer is overridden above.
            // GC.SuppressFinalize(this);
        }
        #endregion
    }
}
