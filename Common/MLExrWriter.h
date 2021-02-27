#pragma once

#include "MLImage.h"
#include "MLConverter.h"

class MLExrWriter {
public:
    MLExrWriter(void) { }
    ~MLExrWriter(void) { }

    /// <summary>
    /// OpenEXR�t�@�C���������B
    /// </summary>
    /// <returns>0:�����B���̐�:���s�B</returns>
    int Write(const char* exrFilePath, const MLImage& img);

private:
    MLConverter mConv;
};
