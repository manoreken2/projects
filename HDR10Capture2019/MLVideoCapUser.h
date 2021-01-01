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
    /// <param name="mtx">�`�惍�b�N�pmutex</param>
    HRESULT Init(std::mutex &mtx);
    void Term(void);

    int NumOfDevices(void) const {
        return mDE.NumOfDevices();
    }
    bool Device(int idx, MLVideoCaptureDeviceEnum::DeviceInf& di_return) {
        return mDE.Device(idx, di_return);
    }

    /// <summary>
    /// �f�o�C�X���g�p�J�n�A�L���v�`���[���n�߂�B
    /// </summary>
    HRESULT UseDevice(IDeckLink* d);

    /// <summary>
    /// �L���v�`���[���~�߁A�f�o�C�X�g�p�I���B
    /// </summary>
    void UnuseDevice(void)
    {
        mVC.StopCapture();
        mVC.SetCallback(nullptr);
        mVC.Term();
    }

    // Use���ɌĂяo����B
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
