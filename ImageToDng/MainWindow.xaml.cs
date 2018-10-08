using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Windows;
using DngRW;
using Microsoft.Win32;

namespace ImageToDng {
    public partial class MainWindow : Window, IDisposable {
        private BackgroundWorker mBackWorker;
        private static int READ_START = 10;
        private static int READ_END = 20;
        private static int WRITE_START = 25;
        private Stopwatch mSW = new Stopwatch();

        public MainWindow() {
            InitializeComponent();
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposing) {
                if (mBackWorker != null) {
                    mBackWorker.Dispose();
                    mBackWorker = null;
                }
            }
        }

        private void buttonBrowseInput_Click(object sender, RoutedEventArgs e) {
            var ofd = new OpenFileDialog();
            var b = ofd.ShowDialog();
            if (b != true) {
                return;
            }

            mTextBoxInputFile.Text = ofd.FileName;
        }

        private void buttonBrowseOutput_Click(object sender, RoutedEventArgs e) {
            var sfd = new SaveFileDialog();
            var b = sfd.ShowDialog();
            if (b != true) {
                return;
            }

            mTextBoxOutputFile.Text = sfd.FileName;
        }

        private DngWriter.CFAPatternType GetCFAPatternType() {
            if (true == mRadioBGGR.IsChecked) {
                return DngWriter.CFAPatternType.BGGR;
            }
            if (true == mRadioGBRG.IsChecked) {
                return DngWriter.CFAPatternType.GBRG;
            }
            if (true == mRadioGRBG.IsChecked) {
                return DngWriter.CFAPatternType.GRBG;
            }
            if (true == mRadioRGGB.IsChecked) {
                return DngWriter.CFAPatternType.RGGB;
            }

            throw new NotImplementedException();
        }

        enum ColorComponentType {
            R,
            G,
            B
        };

        private static ColorComponentType[] mColorsBGGR = new ColorComponentType[] {
            ColorComponentType.B,
            ColorComponentType.G,
            ColorComponentType.G,
            ColorComponentType.R,
        };
        private static ColorComponentType[] mColorsGBRG = new ColorComponentType[] {
            ColorComponentType.G,
            ColorComponentType.B,
            ColorComponentType.R,
            ColorComponentType.G,
        };
        private static ColorComponentType[] mColorsGRBG = new ColorComponentType[] {
            ColorComponentType.G,
            ColorComponentType.R,
            ColorComponentType.B,
            ColorComponentType.G,
        };
        private static ColorComponentType [] mColorsRGGB = new ColorComponentType [] {
            ColorComponentType.R,
            ColorComponentType.G,
            ColorComponentType.G,
            ColorComponentType.B,
        };

        private static ColorComponentType PositionToColorComponent(int x, int y, DngWriter.CFAPatternType ptn) {
            int xIdx = x & 1;
            int yIdx = y & 1;

            int pos = xIdx + yIdx*2;

            switch (ptn) {
            case DngWriter.CFAPatternType.BGGR:
                return mColorsBGGR[pos];
            case DngWriter.CFAPatternType.GBRG:
                return mColorsGBRG[pos];
            case DngWriter.CFAPatternType.GRBG:
                return mColorsGRBG[pos];
            case DngWriter.CFAPatternType.RGGB:
                return mColorsRGGB[pos];
            default:
                throw new NotImplementedException();
            }
        }

        private static byte ColorToSensorValue(System.Drawing.Color c, int x, int y, DngWriter.CFAPatternType ptn) {
            ColorComponentType cc = PositionToColorComponent(x,y,ptn);
            switch (cc) {
            case ColorComponentType.B:
                return c.B;
            case ColorComponentType.G:
                return c.G;
            case ColorComponentType.R:
                return c.R;
            default:
                throw new NotImplementedException();
            }
        }

        class ConvertArgs {
            public string inputPath;
            public string outputPath;
            public DngWriter.CFAPatternType ptn;
            public ConvertArgs(string inPath, string outPath, DngWriter.CFAPatternType aPtn) {
                inputPath = inPath;
                outputPath = outPath;
                ptn = aPtn;
            }
        };

        class ConvertProgressArgs {
            public ConvertArgs args;
            public int imageW;
            public int imageH;
            public ConvertProgressArgs(ConvertArgs aArgs, int w, int h) {
                args = aArgs;
                imageW = w;
                imageH = h;
            }
        }

        class ConvertFinishArgs {
            public string outputPath;
            public bool success;
            public string comment;
            public ConvertFinishArgs(string outPath, bool aSuccess, string aComment) {
                outputPath = outPath;
                success = aSuccess;
                comment = aComment;
            }
        }

        private void buttonConvert_Click(object sender, RoutedEventArgs e) {
            mSW.Start();

            mProgressBar.Value = 0;

            mBackWorker = new BackgroundWorker();
            mBackWorker.WorkerReportsProgress = true;
            mBackWorker.ProgressChanged += new ProgressChangedEventHandler(mBackWorker_ProgressChanged);
            mBackWorker.RunWorkerCompleted += new RunWorkerCompletedEventHandler(mBackWorker_RunWorkerCompleted);
            mBackWorker.DoWork += new DoWorkEventHandler(mBackWorker_DoWork);
            mBackWorker.RunWorkerAsync(new ConvertArgs(mTextBoxInputFile.Text, mTextBoxOutputFile.Text, GetCFAPatternType()));

            mButtonConvert.IsEnabled = false;
        }

        private void ReportProgress(int percentage, bool important, ConvertProgressArgs args) {
            if (important) {
                mBackWorker.ReportProgress(percentage, args);
                mSW.Restart();
                return;
            }

            if (mSW.ElapsedMilliseconds < 1000) {
                return;
            }

            mBackWorker.ReportProgress(percentage, args);
            mSW.Restart();
        }

        void mBackWorker_DoWork(object sender, DoWorkEventArgs e) {
            var args = e.Argument as ConvertArgs;

            e.Result = new ConvertFinishArgs(args.outputPath, false, "");

            ReportProgress(READ_START, true, new ConvertProgressArgs(args, -1, -1));

            try {

                var img = new Bitmap(args.inputPath, true);

                ReportProgress(READ_END, true, new ConvertProgressArgs(args, img.Width, img.Height));

                using (var bw = new BinaryWriter(new FileStream(args.outputPath, FileMode.Create, FileAccess.Write))) {
                    int W = img.Width;
                    int H = img.Height;
                    DngWriter.WriteDngHeader(bw, W, H, 8, args.ptn);

                    for (int y = 0; y < H; ++y) {
                        for (int x = 0; x < W; ++x) {
                            var c = img.GetPixel(x, y);
                            byte b = ColorToSensorValue(c, x, y, args.ptn);
                            bw.Write(b);
                        }
                        double percentage = WRITE_START + (100.0 - WRITE_START) * (y + 1.0) / H;
                        ReportProgress((int)percentage, false, new ConvertProgressArgs(args, img.Width, img.Height));
                    }
                }

                // 成功。
                e.Result = new ConvertFinishArgs(args.outputPath, true, "");

            } catch (Exception ex) {
                e.Result = new ConvertFinishArgs(args.outputPath, false, ex.ToString());
            }

            mSW.Stop();
        }

        void mBackWorker_ProgressChanged(object sender, ProgressChangedEventArgs e) {
            var pa = e.UserState as ConvertProgressArgs;

            if (e.ProgressPercentage == READ_START) {
                AddLog(string.Format("Read Started : {0}\n", pa.args.inputPath));
            }
            if (e.ProgressPercentage == READ_END) {
                AddLog(string.Format("READ End : Image Width={0}, Height={1}\n", pa.imageW, pa.imageH));
                AddLog(string.Format("Write Started : BayerPtn={0}, {1}\n", pa.args.ptn, pa.args.outputPath));
            }

            mProgressBar.Value = e.ProgressPercentage;
        }

        void mBackWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e) {
            var args = e.Result as ConvertFinishArgs;

            if (!args.success) {
                AddLog(string.Format("Error! Conversion Stopped.\n{0}", args.comment));
            } else {
                AddLog(string.Format("Success.\n"));
            }

            mProgressBar.Value = 0;
            mButtonConvert.IsEnabled = true;
        }

        private void AddLog(string s) {
            mTextBoxLog.AppendText(s);
            mTextBoxLog.ScrollToEnd();
        }

        private void TextBox_PreviewDragOver(object sender, DragEventArgs e) {
            e.Handled = true;
        }

        private void textBox_DragEnter(object sender, DragEventArgs e) {
            e.Effects = DragDropEffects.Copy;
        }

        private void textBoxInputFile_Drop(object sender, DragEventArgs e) {
            if (e.Data.GetDataPresent(DataFormats.FileDrop)) {
                var files = (string[])e.Data.GetData(DataFormats.FileDrop);
                mTextBoxInputFile.Text = files[0];
            }
        }

        private void textBoxOutputFile_Drop(object sender, DragEventArgs e) {
            if (e.Data.GetDataPresent(DataFormats.FileDrop)) {
                var files = (string[])e.Data.GetData(DataFormats.FileDrop);
                mTextBoxOutputFile.Text = files[0];
            }
        }
    }
}
