#pragma once

#include "DeckLinkAPI_h.h"
#include <string>

const char* BMDColorspaceToStr(BMDColorspace t);
const char* BMDDynamicRangeToStr(BMDDynamicRange t);
const std::string BMDDetectedVideoInputFormatFlagsToStr(int t);
const std::string BMDDisplayModeFlagsToStr(BMDDisplayModeFlags t);
const char* BMDFieldDominanceToStr(BMDFieldDominance t);
const char* BMDPixelFormatToStr(BMDPixelFormat t);
const char* BMDDisplayModeToStr(BMDDisplayMode t);


