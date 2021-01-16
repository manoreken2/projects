#pragma once

#include "MLVideoCaptureVideoFormat.h"

class IMLVideoCapUserCallback {
public:
    /// <summary>
    /// 入力ビデオフォーマット変更イベント。
    /// </summary>
    virtual void MLVideoCapUserCallback_VideoInputFormatChanged(const MLVideoCaptureVideoFormat& vFmt) = 0;
};
