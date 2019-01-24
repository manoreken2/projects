#pragma once

#include "MLCapturedImage.h"

class MLDrawings {
public:
    enum CrosshairType {
        CH_None,
        CH_CenterCrosshair,
        CH_4Crosshairs,
    };

    enum GridType {
        GR_None,
        GR_3x3,
        GR_6x6,
    };

    void AddCrosshair(MLCapturedImage &ci, CrosshairType ct);
    void AddTitleSafeArea(MLCapturedImage &ci);
    void AddGrid(MLCapturedImage &ci, GridType gt);
};

