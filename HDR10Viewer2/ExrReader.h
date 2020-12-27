#pragma once

#include "MLImage.h"

/// <summary>
/// OpenEXRファイルを読む。
/// </summary>
/// <returns>0:成功。負の数:失敗。</returns>
int ExrRead(const char* exrFilePath, MLImage& img_return);

