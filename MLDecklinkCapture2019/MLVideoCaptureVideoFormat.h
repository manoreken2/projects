#pragma once

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <comutil.h>

#include "DeckLinkAPI_h.h"
#include <vector>

struct MLVideoCaptureVideoFormat {
    int width;
    int height;

    /// <summary>
    /// NTSC, HD1080p5994, ...
    /// </summary>
    BMDDisplayMode displayMode;

    /// <summary>
    /// bmdFormat8BitARGB, bmdFormat10BitRGB, bmdFormat12BitRGB
    /// </summary>
    BMDPixelFormat pixelFormat;

    /// <summary>
    /// frameRateTS / frameRateTV��frames per second�B
    /// </summary>
    BMDTimeValue frameRateTV;
    BMDTimeScale frameRateTS;

    /// <summary>
    /// bmdDisplayModeColorspaceRec709 ���B
    /// ���������̃t�B�[���h������ق����ǂ��B
    /// </summary>
    BMDDisplayModeFlags flags;

    /// <summary>
    /// bmdProgressiveFrame, LowerFieldFirst���B
    /// </summary>
    BMDFieldDominance fieldDominance;

    /// <summary>
    /// SDR, PQ, HLG
    /// </summary>
    BMDDynamicRange dynamicRange;

    /// <summary>
    /// Rec.601, Rec.709, Rec.2020
    /// </summary>
    BMDColorspace colorSpace;

    /*
    // HDR�̃��^�f�[�^�B
    bool hdrMetaExists;
    int eotf;
    float primaryRedX;
    float primaryRedY;
    float primaryGreenX;
    float primaryGreenY;
    float primaryBlueX;
    float primaryBlueY;
    float whitePointX;
    float whitePointY;
    float maxLuma;
    float minLuma;
    float maxContentLightLevel;
    float maxFrameAvgLightLevel;
    */
};
