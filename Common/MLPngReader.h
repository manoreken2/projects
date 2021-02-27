#pragma once

#include "MLImage2.h"

/// <summary>
/// PNGファイルを読む。
/// </summary>
/// <returns>0:成功。負の数:失敗。1:PNGファイルでは無かった。</returns>
int MLPngRead(const wchar_t* pngFilePath, MLImage2& img_return);

