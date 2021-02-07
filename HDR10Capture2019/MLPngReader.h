#pragma once

#include "MLImage.h"

/// <summary>
/// PNGファイルを読む。
/// </summary>
/// <returns>0:成功。負の数:失敗。1:PNGファイルでは無かった。</returns>
int MLPngRead(const wchar_t* pngFilePath, MLImage& img_return);

