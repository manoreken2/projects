#include "MLColorGamut.h"

const char* MLColorGamutToStr(MLColorGamutType t)
{
    switch (t) {
    case     ML_CG_Rec709: return "Rec.709";
    case     ML_CG_AdobeRGB: return "Adobe RGB";
    case     ML_CG_Rec2020: return "Rec.2020";
    }
}
