#pragma once

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <comutil.h>

#include "DeckLinkAPI_h.h"
#include "MLVideoCaptureVideoFormat.h"

class IMLVideoCaptureCallback {
public:
    virtual void MLVideoCaptureCallback_VideoInputFormatChanged(const MLVideoCaptureVideoFormat &vFmt) = 0;
    virtual void MLVideoCaptureCallback_VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioPacket) = 0;
};