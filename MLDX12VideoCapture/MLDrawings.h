#pragma once

#include "MLImage.h"

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

    void AddCrosshair(MLImage &ci, CrosshairType ct);
    void AddTitleSafeArea(MLImage &ci);
    void AddGrid(MLImage &ci, GridType gt);
};

