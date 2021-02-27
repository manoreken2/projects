#pragma once

#include "MLImage.h"
#include "MLConverter.h"

class MLExrWriter {
public:
    MLExrWriter(void) { }
    ~MLExrWriter(void) { }

    /// <summary>
    /// OpenEXRファイルを書く。
    /// </summary>
    /// <returns>0:成功。負の数:失敗。</returns>
    int Write(const char* exrFilePath, const MLImage& img);

private:
    MLConverter mConv;
};
