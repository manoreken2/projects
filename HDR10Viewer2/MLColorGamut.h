#pragma once

enum MLColorGamutType {
    ML_CG_Rec709,
    ML_CG_AdobeRGB,
    ML_CG_Rec2020,
};

const char* MLColorGamutToStr(MLColorGamutType t);
