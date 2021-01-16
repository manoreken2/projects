#include "MLVideoCapUser.h"
#include "DXSampleHelper.h"


HRESULT MLVideoCapUser::Init(std::mutex& mtx)
{
    mMutex = &mtx;
	mDE.Init();

	return S_OK;
}

void MLVideoCapUser::Term(void)
{
	mVC.Term();
	mDE.Term();
    ClearCapturedImageList();
}

static MLConverter::ColorSpace
BmdColorSpaceToMLConverterColorSpace(BMDColorspace cs)
{
    switch (cs) {
    case bmdColorspaceRec601:
        return MLConverter::CS_Rec601;
    case bmdColorspaceRec709:
        return MLConverter::CS_Rec709;
    case bmdColorspaceRec2020:
        return MLConverter::CS_Rec2020;
    default:
        assert(0);
        return MLConverter::CS_Rec601;
    }
}

void
MLVideoCapUser::MLVideoCaptureCallback_VideoInputFrameArrived(
    IDeckLinkVideoInputFrame* videoFrame,
    IDeckLinkAudioInputPacket* audioPacket)
{
    HRESULT hr = S_OK;

    if (audioPacket != nullptr) {
        // オーディオを記録する。
        void* audioBuff = nullptr;
        audioPacket->GetBytes(&audioBuff);

        int audioFrameCount = audioPacket->GetSampleFrameCount();
        if (0 < audioFrameCount) {
            /*
            char s[256];
            sprintf_s(s, "audio frame %d\n", audioFrameCount);
            OutputDebugStringA(s);
            */

            mAviWriter.AddAudio((const uint8_t*)audioBuff, audioFrameCount);
        }
    }

    // 以下はVideoの処理。

    if (videoFrame == nullptr) {
        return;
    }

    void* buffer = nullptr;
    hr = videoFrame->GetBytes(&buffer);
    ThrowIfFailed(hr);

    const int width = videoFrame->GetWidth();
    const int height = videoFrame->GetHeight();
    const int rowBytes = videoFrame->GetRowBytes();
    const BMDPixelFormat fmt = videoFrame->GetPixelFormat();

    if (width <= 0 || height <= 0) {
        // 画像データが届いていない。
        return;
    }

    // 所望の形式の画像データかどうか調査。
    if ((fmt != bmdFormat8BitYUV)
        && (fmt != bmdFormat10BitYUV)
        && (fmt != bmdFormat8BitARGB)
        && (fmt != bmdFormat10BitRGB)
        && (fmt != bmdFormat12BitRGB)) {
        return;
    }

    mAviWriter.AddImage((const uint8_t*)buffer, rowBytes * height);

    const MLVideoCapture::VideoFormat & vFmt = CurrentVideoFormat();

    //const int rowBytes = videoFrame->GetRowBytes();
    MLImage::BitFormatType bft = MLImage::BFT_UIntR8G8B8A8;
    int originalBitDepth = 8;
    int originalNumCh = 4;
    int ml_ImgBytes = width * height * 4; //< 4==A,R,G,B

    MLConverter::ColorSpace colorSpace = BmdColorSpaceToMLConverterColorSpace(vFmt.colorSpace);

    MLColorGamutType gamut = ML_CG_Rec709;
    switch (vFmt.colorSpace) {
    case bmdColorspaceRec601:
        // 対応してない。仮でRec.709ということにする。
        gamut = ML_CG_Rec709;
        break;
    case bmdColorspaceRec709:
        gamut = ML_CG_Rec709;
        break;
    case bmdColorspaceRec2020:
        gamut = ML_CG_Rec2020;
        break;
    }

    MLImage::GammaType gamma = MLImage::MLG_G22;
    switch (vFmt.dynamicRange) {
    case bmdDynamicRangeSDR:
    default:
        gamma = MLImage::MLG_G22;
        break;
    case bmdDynamicRangeHDRStaticPQ:
        gamma = MLImage::MLG_ST2084;
        break;
    case bmdDynamicRangeHDRStaticHLG:
        gamma = MLImage::MLG_HLG;
        break;
    }

    MLImage ci;

    // pixelFormatはMLVideoCapture::VideoInputFormatChangedで制限している。
    switch (fmt) {
    case bmdFormat8BitYUV:
        bft = MLImage::BFT_UIntR8G8B8A8;
        originalBitDepth = 8;
        originalNumCh = 3;
        ml_ImgBytes = width * height * 4; //< 4==R8G8B8A8

        ci.Init(width, height, MLImage::IFFT_CapturedImg,
            bft, gamut, gamma, originalBitDepth, originalNumCh,
            ml_ImgBytes, new uint8_t[ml_ImgBytes]);
        mConv.Uyvy8bitToR8G8B8A8(colorSpace, (uint32_t*)buffer, (uint32_t*)ci.data, width, height);
        break;
    case bmdFormat10BitYUV:
        bft = MLImage::BFT_UIntR10G10B10A2;
        originalBitDepth = 10;
        originalNumCh = 3;
        ml_ImgBytes = width * height * 4; //< 4==R10G10B10A2

        ci.Init(width, height, MLImage::IFFT_CapturedImg,
            bft, gamut, gamma, originalBitDepth, originalNumCh,
            ml_ImgBytes, new uint8_t[ml_ImgBytes]);
        mConv.Yuv422_10bitToR10G10B10A2(colorSpace, (uint32_t*)buffer, (uint32_t*)ci.data, width, height);
        break;
    case bmdFormat8BitARGB:
        bft = MLImage::BFT_UIntR8G8B8A8;
        originalBitDepth = 8;
        originalNumCh = 4;
        ml_ImgBytes = width * height * 4; //< 4==R8B8G8A8

        ci.Init(width, height, MLImage::IFFT_CapturedImg,
            bft, gamut, gamma, originalBitDepth, originalNumCh,
            ml_ImgBytes, new uint8_t[ml_ImgBytes]);
        mConv.Argb8bitToR8G8B8A8((uint32_t*)buffer, (uint32_t*)ci.data, width, height);
        break;
    case bmdFormat10BitRGB:
        bft = MLImage::BFT_UIntR10G10B10A2;
        originalBitDepth = 10;
        originalNumCh = 3;
        ml_ImgBytes = width * height * 4; //< 4==R10G10B10A2

        ci.Init(width, height, MLImage::IFFT_CapturedImg,
            bft, gamut, gamma, originalBitDepth, originalNumCh,
            ml_ImgBytes, new uint8_t[ml_ImgBytes]);
        mConv.Rgb10bitToR10G10B10A2((uint32_t*)buffer, (uint32_t*)ci.data, width, height, 0xff);
        break;
    case bmdFormat12BitRGB:
        bft = MLImage::BFT_UIntR10G10B10A2;
        originalBitDepth = 12;
        originalNumCh = 3;
        ml_ImgBytes = width * height * 4; //< 4==R10G10B10A2

        ci.Init(width, height, MLImage::IFFT_CapturedImg,
            bft, gamut, gamma, originalBitDepth, originalNumCh,
            ml_ImgBytes, new uint8_t[ml_ImgBytes]);
        mConv.Rgb12bitToR10G10B10A2((uint32_t*)buffer, (uint32_t*)ci.data, width, height, 0xff);
        break;
    }

    mMutex->lock();

    while (CapturedImageQueueSize <= mCapturedImages.size()) {
        // 詰まっているとき。古いデータを消す。
        ++mFrameSkipCount;

        MLImage & img = mCapturedImages.front();
        img.Term();
        mCapturedImages.pop_front();
    }

    // キャプチャー画像を追加。
    mCapturedImages.push_back(ci);
    mMutex->unlock();
}

HRESULT
MLVideoCapUser::UseDevice(IDeckLink* d)
{
	HRESULT hr = S_OK;

	if (!mVC.Init(d)) {
		return E_FAIL;
	}

	mVC.SetCallback(this);

	hr = mVC.StartCapture(0);
	if (FAILED(hr)) {
		return hr;
	}

	return S_OK;
}

int
MLVideoCapUser::CapturedImageCount(void)
{
    int r = 0;

    mMutex->lock();

    r = (int)mCapturedImages.size();

    mMutex->unlock();

    return r;
}

HRESULT
MLVideoCapUser::CreateCopyOfCapturedImg(MLImage& img_return)
{
    HRESULT hr = S_OK;

    mMutex->lock();

    //printf("%d \n", (int)mCapturedImages.size());

    if (mCapturedImages.size() == 0) {
        hr = E_FAIL;
    } else {
        assert(img_return.data == nullptr);
        // 画像メモリ領域dataの実体のコピーを作成する。
        // popしない。
        img_return.DeepCopyFrom(mCapturedImages.front());

        if (img_return.data != nullptr) {
            hr = S_OK;
        } else {
            // 画像が空である。
            hr = E_FAIL;
        }
    }

    mMutex->unlock();

    return hr;
}

HRESULT
MLVideoCapUser::PopCapturedImg(MLImage& img_return)
{
    HRESULT hr = S_OK;

    assert(img_return.data == nullptr);

    mMutex->lock();

    if (mCapturedImages.size() == 0) {
        hr = E_FAIL;
    } else {
        img_return = mCapturedImages.front();
        mCapturedImages.pop_front();
        hr = S_OK;
    }

    mMutex->unlock();

    return hr;
}

HRESULT
MLVideoCapUser::FlushStreams(void)
{
    mFrameSkipCount = 0;
    return mVC.FlushStreams();
}

void
MLVideoCapUser::ClearCapturedImageList(void)
{
    mMutex->lock();

    while (0 < mCapturedImages.size()) {
        MLImage & img = mCapturedImages.front();
        img.Term();

        mCapturedImages.pop_front();
    }

    mMutex->unlock();
}



