#pragma once

#include "MLImage2.h"

/// <summary>
/// OpenEXRファイルを読む。
/// </summary>
/// <returns>0:成功。負の数:失敗。</returns>
int MLExrRead(const char* exrFilePath, MLImage2& img_return);

