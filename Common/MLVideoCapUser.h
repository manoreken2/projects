#pragma once

#include "IMLVideoCaptureCallback.h"
#include "MLVideoCapture.h"
#include "MLVideoCaptureDeviceEnum.h"
#include "MLImage.h"
#include <list>
#include <mutex>
#include "MLConverter.h"
#include "MLAviWriter.h"
#include "IMLVideoCapUserCallback.h"


class MLVideoCapUser : IMLVideoCaptureCallback {
public:
    /// <param name="mtx">�`�惍�b�N�pmutex</param>
    HRESULT Init(std::mutex &mtx);
    void Term(void);

    void SetCallback(IMLVideoCapUserCallback* cb) {
        mVCU_cb = cb;
    }

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
    const MLVideoCaptureVideoFormat & CurrentVideoFormat(void) const {
        return mVC.CurrentVideoFormat();
    }

    int FrameSkipCount(void) const {
        return mFrameSkipCount;
    }

    /// <summary>
    /// �L���[����Ă���\���p�L���v�`���[�摜�̐��B
    /// </summary>
    int CapturedImageCount(void);

    void UpdateImageGamma(MLImage::GammaType g);
    HRESULT PopCapturedImg(MLImage& img_return);
    HRESULT CreateCopyOfCapturedImg(MLImage& img_return);

    BMDDetectedVideoInputFormatFlags DetectedVideoInputFormatFlags(void) const {
        return mVC.DetectedVideoInputFormatFlags();
    }
    MLAviWriter & AviWriter(void) { return mAviWriter; }

    HRESULT FlushStreams(void);
    void ClearCapturedImageList(void);

private:
    std::mutex* mMutex = nullptr;
    void MLVideoCaptureCallback_VideoInputFormatChanged(const MLVideoCaptureVideoFormat & vFmt);
    void MLVideoCaptureCallback_VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioPacket);
    MLVideoCaptureDeviceEnum mDE;
    MLVideoCapture mVC;
    std::list<MLImage> mCapturedImages;
    int mFrameSkipCount = 0;

    MLConverter mConv;
    MLAviWriter mAviWriter;

    const int CapturedImageQueueSize = 2;

    IMLVideoCapUserCallback * mVCU_cb = nullptr;
};
