using System;
using System.Diagnostics;
using System.IO;

namespace DngRW {
    [DebuggerDisplay("{tag} {fieldType} {count} {dataOffset}")]
    public class IFDEntry {
        public enum Tag {
            Unknown = -1,
            End = 0,
            NewSubfileType = 0xfe, //< Mandatory
            ImageWidth = 0x100, //< Mandatory
            ImageLength = 0x101, //< Mandatory
            BitsPerSample = 0x102, //< Mandatory
            Compression = 0x103, //< Mandatory
            PhotometricInterpretation = 0x106, //< Mandatory
            Thresholding = 0x107,
            CellWidth = 0x108,
            CellLength = 0x109,
            FillOrder = 0x10a,
            DocumentName = 0x10d,
            ImageDescription = 0x10e, //< Mandatory
            Make = 0x10f, //< Mandatory
            Model = 0x110, //< Mandatory
            StripOffsets = 0x111, //< Mandatory
            Orientation = 0x112, //< Mandatory
            SamplesPerPixel = 0x115, //< Mandatory
            RowsPerStrip = 0x116, //< Mandatory
            StripByteCounts = 0x117, //< Mandatory
            MinSampleValue = 0x118,
            MaxSampleValue = 0x119,
            XResolution = 0x11a, //< Mandatory
            YResolution = 0x11b, //< Mandatory
            PlanarConfiguration = 0x11c, //< Mandatory
            PageName = 0x11d,
            XPosition = 0x11e,
            YPosition = 0x11f,
            FreeOffsets = 0x120,
            FreeByteCounts = 0x121,
            GrayResponseUnit = 0x122,
            GrayResponseCurve = 0x123,
            T4Options = 0x124,
            T6Options = 0x125,
            ResolutionUnit = 0x128, //< Mandatory
            PageNumber = 0x129,
            TransferFunction = 0x12d,
            Software = 0x131, //< Mandatory
            DateTime = 0x132, //< Mandatory
            Artist = 0x13b,
            HostComputer = 0x13c,
            Predictor = 0x13d,
            WhitePoint = 0x13e,
            PrimaryChromaticities = 0x13f,
            Colormap = 0x140,
            HalftoneHints = 0x141,
            TileWidth = 0x142,
            TileLength = 0x143,
            TileOffsets = 0x144,
            TileByteCounts = 0x145,
            SubIFDs = 0x14a,
            InkSet = 0x14c,
            InkNames = 0x14d,
            NumberOfInks = 0x14e,
            DotRange = 0x150,
            TargetPrinter = 0x151,
            ExtraSamples = 0x152,
            SampleFormat = 0x153,
            SMinSampleValue = 0x154,
            SMaxSampleValue = 0x155,
            TransferRange = 0x156,
            JPEGProc = 0x200,
            JPEGInterchangeFormat = 0x201,
            JPEGInterchangeFormatLength = 0x202,
            JPEGRestartInterval = 0x203,
            JPEGLosslessPredictors = 0x205,
            JPEGPointTransforms = 0x206,
            JPEGQTables = 0x207,
            JPEGDCTables = 0x208,
            JPEGACTables = 0x209,
            YCbCrCoefficients = 0x211,
            YCbCrSubSampling = 0x212,
            YCbCrPositioning = 0x213,
            ReferenceBlackWhite = 0x214,
            WangAnnotation=0x80a4,
            CFARepeatPatternDim = 0x828d,
            CFAPattern = 0x828e,
            BatteryLevel=0x828f,
            Copyright = 0x8298, //< Mandatory
            ExposureTime = 0x829a,
            FNumber=0x829d,
            MDFileTag = 0x82a5,
            MDScalePixel = 0x82a6,
            MDColorTable = 0x82a7,
            MDLabName=0x82a8,
            MDSampleInfo=0x82a9,
            MDPrepDate=0x82aa,
            MDPrepTime=0x82ab,
            MDFileUnits=0x82ac,
            ModelPixelScaleTag=0x830e,
            IPTCNAA = 0x83bb,
            INGRPacketDataTag=0x847e,
            INGRFlagRegisters=0x847f,
            IrasBTransformationMatrix=0x8480,
            ModelTiepointTag=0x8482,
            ModelTransformationTag=0x85d8,
            Photoshop=0x8649,
            ExifIFD=0x8769,
            InterColorProfile=0x8773,
            GeoKeyDirectoryTag=0x87af,
            GeoDoubleParamsTag=0x87b0,
            GeoAsciiParamsTag=0x87b1,
            ExposureProgram=0x8822,
            SpectralSensitivity=0x8824,
            GPSInfo=0x8825,
            ISOSpeedRatings=0x8827,
            OECF=0x8828,
            Interlace=0x8829,
            TimeZoneOffset=0x882a,
            SelfTimerMode=0x882b,
            HylaFAXFaxRecvParams=0x885c,
            HylaFAXFaxSubAddress=0x885d,
            HylaFAXFaxRecvTime=0x885e,
            ExifVersion=0x9000,
            DateTimeOriginal = 0x9003, //< Mandatory
            DateTimeDigitized=0x9004,
            ComponentsConfiguration=0x9101,
            CompressedBitsPerPixel=0x9102,
            ShutterSpeedValue=0x9201,
            ApertureValue=0x9202,
            BrightnessValue=0x9203,
            ExposureBiasValue=0x9204,
            MaxApertureValue=0x9205,
            SubjectDistance=0x9206,
            MeteringMode=0x9207,
            LightSource=0x9208,
            Flash=0x9209,
            FocalLength=0x920a,
            FlashEnergy=0x920b,
            SpacialFrequencyResponse=0x920c,
            Noise=0x920d,
            FocalPlaneXResolution=0x920e,
            FocalPlaneYResolution=0x920f,
            FocalPlaneResolutionUnit=0x9210,
            ImageNumber=0x9211,
            SecurityClassification=0x9212,
            ImageHistory=0x9213,
            SubjectLocation=0x9214,
            ExposureIndex=0x9215,
            TiffEpStandardID=0x9216,  //< Mandatory
            SensingMethod=0x9217,
            MakerNote=0x927c,
            UserComment=0x9286,
            SubsecTime=0x9290,
            SubsecTimeOriginal=0x9291,
            SubsecTimeDigitized=0x9292,
            ImageSourceData=0x935c,

            FlashpixVersion=0xa000,
            ColorSpaceA=0xa001,
            PixelXDimensionA=0xa002,
            PixelYDimensionA=0xa003,
            RelatedSourceFileA=0xa004,
            InteroperabilityIFD=0xa005,
            FlashEnergyA=0xa20b,
            SpatialFrequencyResponseA=0xa20c,
            FocalPlaneXResolutionA=0xa20e,
            FocalPlaneYResolutionA=0xa20f,
            FocalPlaneResolutionUnitA=0xa210,
            SubjectLocationA=0xa214,
            ExposureIndexA=0xa215,
            SensingMethodA=0xa217,
            FileSourceA=0xa300,
            SceneTypeA=0xa301,
            CFAPatternA=0xa302,
            CustomRenderedA=0xa401,
            ExposureModeA=0xa402,
            WhiteBalanceA=0xa403,
            DigitalZoomRatioA=0xa404,
            FocalLengthIn35mmFilmA=0xa405,
            SceneCaptureTypeA=0xa406,
            GainControlA=0xa407,
            ContrastA=0xa408,
            SaturationA=0xa409,
            SharpnessA=0xa40a,
            DeviceSettingDescriptionA=0xa40b,
            SubjectDistanceRange=0xa40c,
            ImageUniqueIDA=0xa420,
            CameraSerialNumberA=0xa431,
            LensInfoA=0xa432,
            LensNameA=0xa434,
            GDALMetadata=0xa480,
            GDALNoData=0xa481,
            OceScanjobDescription=0xc427,
            OceApplicationSelector=0xc428,
            OceIdentificationNumber=0xc429,
            OceImageLogicCharacteristics=0xc42a,
            // DNG
            DNGVersion = 0xc612,
            DNGBackwardVersion = 0xc613,
            UniqueCameraModel = 0xc614,
            LocalizedCameraModel = 0xc615,
            CFAPlaneColor = 0xc616,
            CFALayout = 0xc617,
            LinearizationTable = 0xc618,
            BlackLevelRepeatDim = 0xc619,
            BlackLevel = 0xc61a,
            BlackLevelDeltaH = 0xc61b,
            BlackLevelDeltaV = 0xc61c,
            WhiteLevel = 0xc61d,
            DefaultScale = 0xc61e,
            BestQualityScale = 0xc65c,
            DefaultCropOrigin = 0xc61f,
            DefaultCropSize = 0xc620,
            CalibrationIlluminant1 = 0xc65a,
            CalibrationIlluminant2 = 0xc65b,
            ColorMatrix1 = 0xc621,
            ColorMatrix2 = 0xc622,
            CameraCalibration1 = 0xc623,
            CameraCalibration2 = 0xc624,
            ReductionMatrix1 = 0xc625,
            ReductionMatrix2 = 0xc626,
            AnalogBalance = 0xc627,
            AsShotNeutral = 0xc628,
            AsShotWhiteXY = 0xc629,
            BaselineExposure = 0xc62a,
            BaselineNoise = 0xc62b,
            BaselineSharpness = 0xc62c,
            BayerGreenSplit = 0xc62d,
            LinearResponseLimit = 0xc62e,
            CameraSerialNumber = 0xc62f,
            LensInfo = 0xc630,
            ChromaBlurRadius = 0xc631,
            AntiAliasStrength = 0xc632,
            ShadowScale = 0xc633,
            DNGPrivateData = 0xc634,
            MakerNoteSafety = 0xc635,
            RawDataUniqueID = 0xc65d,
            OriginalRawFileName = 0xc68b,
            OriginalRawFileData = 0xc68c,
            ActiveArea = 0xc68d,
            MaskedAreas = 0xc68e,
            AsShotICCProfile = 0xc68f,
            AsShotPreProfileMatrix = 0xc690,
            CurrentICCProfile = 0xc691,
            CurrentPreProfileMatrix = 0xc692,
            ColorimetricReference = 0xc6bf,
            GeoMetadata=0xc6dd,
            CameraCalibrationSignature = 0xc6f3,
            ProfileCalibrationSignature = 0xc6f4,
            ExtraCameraProfiles = 0xc6f5,
            AsShotProfileName = 0xc6f6,
            NoiseReductionApplied = 0xc6f7,
            ProfileName = 0xc6f8,
            ProfileHueSatMapDims = 0xc6f9,
            ProfileHueSatMapData1 = 0xc6fa,
            ProfileHueSatMapData2 = 0xc6fb,
            ProfileToneCurve = 0xc6fc,
            ProfileEmbedPolicy = 0xc6fd,
            ProfileCopyright = 0xc6fe,
            ForwardMatrix1 = 0xc714,
            ForwardMatrix2 = 0xc715,
            PreviewApplicationName = 0xc716,
            PreviewApplicationVersion = 0xc717,
            PreviewSettingsName = 0xc718,
            PreviewSettingsDigest = 0xc719,
            PreviewColorSpace = 0xc71a,
            PreviewDateTime = 0xc71b,
            RawImageDigest = 0xc71c,
            OriginalRawFileDigest = 0xc71d,
            SubTileBlockSize = 0xc71e,
            RowInterleaveFactor = 0xc71f,
            ProfileLookTableDims = 0xc725,
            ProfileLookTableData = 0xc726,
            OpcodeList1 = 0xc740,
            OpcodeList2 = 0xc741,
            OpcodeList3 = 0xc74e,
            NoiseProfile = 0xc761,
            DefaultUserCrop = 0xc7b5,
            DefaultBlackRender = 0xc7a6,
            BaselineExposureOffset = 0xc7a5,
            ProfileLookTableEncoding = 0xc7a4,
            ProfileHueSatMapEncoding = 0xc7a3,
            OriginalDefaultFinalSize = 0xc791,
            OriginalBestQualityFinalSize = 0xc792,
            OriginalDefaultCropSize = 0xc793,
            NewRawImageDigest = 0xc7a7,
            RawToPreviewGain = 0xc7a8,

            // GPS (added 0x10000)
            GpsVersionID = 0x10000,
            GpsLatitudeRef=0x10001,
            GpsLatitude=0x10002,
            GpsLongitudeRef=0x10003,
            GpsLongitude=0x10004,
            GpsAltitudeRef=0x10005,
            GpsAltitude=0x10006,
            GpsTimeStamp=0x10007,
            GpsSatellites=0x10008,
            GpsStatus=0x10009,
            GpsMeasureMode=0x1000a,
            GpsDop=0x1000b,
            GpsSpeedRef=0x1000c,
            GpsSpeed=0x1000d,
            GpsTrackRef=0x1000e,
            GpsTrack=0x1000f,
            GpsImgDirectionRef=0x10010,
            GpsImgDirection=0x10011,
            GpsMapDatum=0x10012,
            GpsDestLatitudeRef=0x10013,
            GpsDestLatitude=0x10014,
            GpsDestLongitudeRef=0x10015,
            GpsDestLongitude=0x10016,
            GpsDestBearingRef=0x10017,
            GpsDestBearing=0x10018,
            GpsDestDistanceRef=0x10019,
            GpsDestDistance=0x1001a,
            GpsProcessingMethod=0x1001b,
            GpsAreaInformation=0x1001c,
            GpsDateStamp=0x1001d,
            GpsDifferential=0x1001e,

        };

        public enum FieldType {
            Unknown = -1,
            BYTE = 1,
            ASCII = 2,
            SHORT = 3,
            LONG = 4,
            RATIONAL = 5,
            SBYTE = 6,
            UNDEFINED = 7,
            SSHORT = 8,
            SLONG = 9,
            SRATIONAL = 10,
            FLOAT = 11,
            DOUBLE = 12
        };

        public enum CompressionType {
            Uncompressed = 1,
            JPEGCompressedMaybeLossless=7,
            ZIPCompressed=8,
            LossyJPEGCompressed=34892,
        };

        public enum PhotometricInterpretationType {
            BlackIsZero = 1,
            RGB = 2,
            YCbCr = 6,
            ColorFilterArray = 32803, //< pre-bayer image
            LinearRaw = 34892, //< camera does not use color filter array
        };

        public enum PlanarConfigurationType {
            Chunky = 1,
            Planar = 2,
        };

        public enum ResolutionUnitType {
            NoAbsoluteUnit = 1,
            Inch = 2,
            Centimeter = 3,
        };
        
        public enum SampleFormatType {
            Uint = 1,
            Float = 3,
        };

        public enum OrientationType {
            TopLeft = 1,
        };

        public enum CFALayoutType {
            RectangularOrSquare = 1,
            StaggeredLayoutA = 2,
            StaggeredLayoutB = 3,
            StaggeredLayoutC = 4,
            StaggeredLayoutD = 5,
            StaggeredLayoutE = 6,
            StaggeredLayoutF = 7,
            StaggeredLayoutG = 8,
            StaggeredLayoutH = 9,
        };

        public enum CFAPatternType {
            R = 0,
            G = 1,
            B = 2,
            Cyan = 3,
            Magenta = 4,
            Yellow = 5,
            White = 6,
        };

        // IFDEntry.tag (0バイト目から2バイト)
        public Tag tag;
        // IFDEntry.fieldType (2バイト目から2バイト)
        public FieldType fieldType;
        // IFDEntry.count (4バイト目から4バイト)
        public int count;
        // IFDEntry.dataOffset(8バイト目から4バイト)
        public uint dataOffset;

        // IFDEntryを書き込んだ場所(ファイルの先頭からのオフセットバイト)
        public long ifdEntryOffset;

        // データはpadされていない。
        public byte[] data;

        // ファイル先頭からのオフセットバイト数。(option)
        public long filePosition;
        // ファイルに入っていたタグの数値 (option)
        public uint originalTag;

        public static int DataBytes(FieldType ft, int count) {
            switch (ft) {
            case FieldType.ASCII:
            case FieldType.BYTE:
            case FieldType.SBYTE:
            case FieldType.UNDEFINED:
                return count;
            case FieldType.SHORT:
            case FieldType.SSHORT:
                return count * 2;
            case FieldType.FLOAT:
            case FieldType.LONG:
            case FieldType.SLONG:
                return count * 4;
            case FieldType.DOUBLE:
            case FieldType.RATIONAL:
            case FieldType.SRATIONAL:
                return count * 8;
            case FieldType.Unknown:
            default:
                return 0;
            }
        }

        public IFDEntry(Tag t, FieldType ft, int c) {
            tag = t;
            fieldType = ft;
            count = c;
            dataOffset = 0;
            data = new byte[0];
        }

        public IFDEntry(Tag t, FieldType ft, int c, double[] d) {
            tag = t;
            fieldType = ft;
            count = c;

            System.Diagnostics.Debug.Assert(c == d.Length);

            dataOffset = 0; //< 後でWriteData()で決まる。
            data = new byte[d.Length * 8];
            int pos = 0;
            for (int i = 0; i < d.Length; ++i) {
                var b = BitConverter.GetBytes(d[i]);
                Array.Copy(b, 0, data, pos, 8);
                pos += 8;
            }
        }

        public IFDEntry(Tag t, FieldType ft, int c, float[] d) {
            tag = t;
            fieldType = ft;
            count = c;

            System.Diagnostics.Debug.Assert(c == d.Length);

            if (1 < d.Length) {
                dataOffset = 0; //< 後でWriteData()で決まる。
                data = new byte[d.Length * 4];
                int pos = 0;
                for (int i = 0; i < d.Length; ++i) {
                    var b = BitConverter.GetBytes(d[i]);
                    Array.Copy(b, 0, data, pos, 4);
                    pos += 4;
                }
            } else {
                var f = BitConverter.GetBytes(d[0]);
                dataOffset = (uint)(f[0] + (f[1] << 8) + (f[2] << 16) + (f[3] << 24));
                data = f;
            }
        }

        public IFDEntry(Tag t, FieldType ft, int c, int[] d) {
            tag = t;
            fieldType = ft;
            count = c;

            System.Diagnostics.Debug.Assert(c == d.Length);

            if (1 < d.Length) {
                dataOffset = 0; //< 後でWriteData()で決まる。
                data = new byte[d.Length * 4];
                int pos = 0;
                for (int i = 0; i < d.Length; ++i) {
                    var b = BitConverter.GetBytes(d[i]);
                    Array.Copy(b, 0, data, pos, 4);
                    pos += 4;
                }
            } else {
                dataOffset = (uint)d[0];
                data = BitConverter.GetBytes(d[0]);
            }
        }

        public IFDEntry(Tag t, FieldType ft, int c, uint[] d) {
            tag = t;
            fieldType = ft;
            count = c;

            System.Diagnostics.Debug.Assert(c == d.Length);

            if (1 < d.Length) {
                dataOffset = 0; //< 後でWriteData()で決まる。
                data = new byte[d.Length * 4];
                int pos = 0;
                for (int i = 0; i < d.Length; ++i) {
                    var b = BitConverter.GetBytes(d[i]);
                    Array.Copy(b, 0, data, pos, 4);
                    pos += 4;
                }
            } else {
                dataOffset = d[0];
                data = BitConverter.GetBytes(d[0]);
            }
        }

        public IFDEntry(Tag t, FieldType ft, int c, short[] d) {
            tag = t;
            fieldType = ft;
            count = c;

            System.Diagnostics.Debug.Assert(c == d.Length);

            if (2 < d.Length) {
                dataOffset = 0; //< 後でWriteData()で決まる。
                data = new byte[d.Length * 2];
                int pos = 0;
                for (int i = 0; i < d.Length; ++i) {
                    var b = BitConverter.GetBytes(d[i]);
                    Array.Copy(b, 0, data, pos, 2);
                    pos += 2;
                }
            } else {
                if (count == 1) {
                    ushort v = (ushort)d[0];
                    dataOffset = v;
                } else {
                    // count==2
                    uint v0 = (ushort)d[0];
                    uint v1 = (ushort)d[1];
                    dataOffset = v0 + (v1 << 16);
                }
                data = BitConverter.GetBytes(dataOffset);
            }
        }

        public IFDEntry(Tag t, FieldType ft, int c, ushort[] d) {
            tag = t;
            fieldType = ft;
            count = c;

            System.Diagnostics.Debug.Assert(c == d.Length);

            if (2 < d.Length) {
                dataOffset = 0; //< 後でWriteData()で決まる。
                data = new byte[d.Length * 2];
                int pos = 0;
                for (int i = 0; i < d.Length; ++i) {
                    var b = BitConverter.GetBytes(d[i]);
                    Array.Copy(b, 0, data, pos, 2);
                    pos += 2;
                }
            } else {
                if (count == 1) {
                    dataOffset = d[0];
                } else {
                    // count == 2
                    uint v0 = d[0];
                    uint v1 = d[1];
                    dataOffset = v0 + (v1<<16);
                }
                data = BitConverter.GetBytes(dataOffset);
            }
        }

        public IFDEntry(Tag t, FieldType ft, int c, byte[] d) {
            tag = t;
            fieldType = ft;
            count = c;

            System.Diagnostics.Debug.Assert(c == d.Length);

            if (4 < d.Length) {
                dataOffset = 0; //< 後でWriteData()で決まる。
                data = new byte[d.Length];
                Array.Copy(d, data, d.Length);
            } else {
                data = d;
                dataOffset = d[0];
                if (2 <= count) {
                    uint d1 = d[1];
                    dataOffset += d1 << 8;
                }
                if (3 <= count) {
                    uint d2 = d[2];
                    dataOffset += d2 << 16;
                }
                if (4 <= count) {
                    uint d3 = d[3];
                    dataOffset += d3 << 24;
                }
            }
        }

        public IFDEntry(Tag t, FieldType ft, int c, IFDRational[] d) {
            tag = t;
            fieldType = ft;
            count = c;

            System.Diagnostics.Debug.Assert(c == d.Length);

            dataOffset = 0; //< 後でWriteData()で決まる。
            data = new byte[d.Length * 8];
            int pos = 0;
            for (int i = 0; i < d.Length; ++i) {
                var numer = BitConverter.GetBytes(d[i].numer);
                var denom = BitConverter.GetBytes(d[i].denom);
                Array.Copy(numer, 0, data, pos, 4);
                pos += 4;
                Array.Copy(denom, 0, data, pos, 4);
                pos += 4;
            }
        }

        public IFDEntry(Tag t, FieldType ft, int c, int d)
            : this(t, ft, c, new int[] { d }) {
        }

        public IFDEntry(Tag t, FieldType ft, int c, short d)
            : this(t, ft, c, new short[] { d }) {
        }

        public IFDEntry(Tag t, FieldType ft, int c, IFDRational d)
            : this(t, ft, c, new IFDRational[] { d }) {
        }

        /// <summary>
        /// データのバイト数。パッド含まず。
        /// </summary>
        public int DataBytes() {
            return data.Length;
        }

        public uint[] GetDataAsUIntArray() {
            switch (fieldType) {
            case FieldType.LONG:
                var rv = new uint[count];
                for (int i = 0; i < count; ++i) {
                    rv[i] = BitConverter.ToUInt32(data, i * 4);
                }
                return rv;
            default:
                throw new FormatException();
            }
        }

        public ushort[] GetDataAsUShortArray() {
            switch (fieldType) {
            case FieldType.SHORT:
                var rv = new ushort[count];
                for (int i = 0; i < count; ++i) {
                    rv[i] = BitConverter.ToUInt16(data, i * 2);
                }
                return rv;
            default:
                throw new FormatException();
            }
        }

        public byte[] GetDataAsByteArray() {
            switch (fieldType) {
            case FieldType.BYTE:
                return data;
            default:
                throw new FormatException();
            }
        }

        /*
        public int[] GetDataAsSIntArray() {
            switch (fieldType) {
            case FieldType.SLONG:
                var rv = new int[count];
                for (int i = 0; i < count; ++i) {
                    rv[i] = BitConverter.ToInt32(data, i * 4);
                }
                return rv;
            default:
                throw new FormatException();
            }
        }
        */
        
        /// <summary>
        /// IFDエントリーを書き込む。(12バイト)
        /// </summary>
        public void WriteEntry(BinaryWriter bw) {
            ifdEntryOffset = bw.BaseStream.Position;
            WriteUshort(bw, (ushort)tag);
            WriteUshort(bw, (ushort)fieldType);
            WriteInt(bw, count);
            WriteUInt(bw, dataOffset);
        }

        /// <summary>
        /// データを書き込み、IFDのオフセットを更新。
        /// </summary>
        /// <returns>書き込んだバイト数(パッド含む)。</returns>
        public int WriteData(BinaryWriter bw) {
            if (data.Length <= 4) {
                return 0;
            }

            dataOffset = (uint)bw.BaseStream.Position;

            // サイズが4バイトの倍数になるように書き込む。
            int paddedDataBytes = (data.Length + 3) & (~3);
            int padBytes = paddedDataBytes - data.Length;
            WriteBytes(bw, data);
            for (int i = 0; i < padBytes; ++i) {
                byte z = 0;
                WriteByte(bw, z);
            }

            long currentPos = bw.BaseStream.Position;
            bw.BaseStream.Seek(ifdEntryOffset+8, SeekOrigin.Begin);

            WriteUInt(bw, dataOffset);

            bw.BaseStream.Seek(currentPos, SeekOrigin.Begin);
            return paddedDataBytes;
        }

        private static void WriteByte(BinaryWriter bw, byte v) {
            bw.Write(v);
        }

        private static void WriteBytes(BinaryWriter bw, byte[] v) {
            bw.Write(v);
        }

        private static void WriteUshort(BinaryWriter bw, ushort v) {
            bw.Write(v);
        }

        private static void WriteShort(BinaryWriter bw, short v) {
            bw.Write(v);
        }

        private static void WriteInt(BinaryWriter bw, int v) {
            bw.Write(v);
        }

        private static void WriteUInt(BinaryWriter bw, uint v) {
            bw.Write(v);
        }
    }
}
