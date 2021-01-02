#pragma once

#include "MLImage.h"

/// <summary>
/// OpenEXRファイルを書く。
/// </summary>
/// <returns>0:成功。負の数:失敗。</returns>
int MLExrWrite(const char* exrFilePath, const MLImage& img);

