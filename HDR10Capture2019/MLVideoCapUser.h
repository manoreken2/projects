#pragma once

#include "IMLVideoCaptureCallback.h"
#include "MLVideoCapture.h"
#include "MLVideoCaptureDeviceEnum.h"
#include "MLImage.h"
#include <list>
#include <mutex>
#include "MLConverter.h"

class MLVideoCapUser : IMLVideoCaptureCallback {
public:
    /// <param name="mtx">描画ロック用mutex</param>
    HRESULT Init(std::mutex &mtx);
    void Term(void);

    int NumOfDevices(void) const {
        return mDE.NumOfDevices();
    }
    bool Device(int idx, MLVideoCaptureDeviceEnum::DeviceInf& di_return) {
        return mDE.Device(idx, di_return);
    }

    /// <summary>
    /// デバイスを使用開始、キャプチャーを始める。
    /// </summary>
    HRESULT UseDevice(IDeckLink* d);

    /// <summary>
    /// キャプチャーを止め、デバイス使用終了。
    /// </summary>
    void UnuseDevice(void)
    {
        mVC.StopCapture();
        mVC.SetCallback(nullptr);
        mVC.Term();
    }

    // Use中に呼び出せる。
    const MLVideoCapture::VideoFormat & CurrentVideoFormat(void) const {
        return mVC.CurrentVideoFormat();
    }

    const int CapturedImageQueueSize = 1;

    int FrameSkipCount(void) const {
        return mFrameSkipCount;
    }

    HRESULT PopCapturedImg(MLImage& img_return);

    BMDDetectedVideoInputFormatFlags DetectedVideoInputFormatFlags(void) const {
        return mVC.DetectedVideoInputFormatFlags();
    }

private:
    std::mutex* mMutex = nullptr;
    void MLVideoCaptureCallback_VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioPacket);
    MLVideoCaptureDeviceEnum mDE;
    MLVideoCapture mVC;
    std::list<MLImage> mCapturedImages;
    int mFrameSkipCount = 0;

    MLConverter mConv;
};
