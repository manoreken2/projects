#pragma once

#include "MLImage.h"

/// <summary>
/// OpenEXR�t�@�C����ǂށB
/// </summary>
/// <returns>0:�����B���̐�:���s�B</returns>
int MLExrRead(const char* exrFilePath, MLImage& img_return);

