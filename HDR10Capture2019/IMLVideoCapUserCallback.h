#pragma once

#include "MLVideoCaptureVideoFormat.h"

class IMLVideoCapUserCallback {
public:
    /// <summary>
    /// ���̓r�f�I�t�H�[�}�b�g�ύX�C�x���g�B
    /// </summary>
    virtual void MLVideoCapUserCallback_VideoInputFormatChanged(const MLVideoCaptureVideoFormat& vFmt) = 0;
};
