#pragma once

#include "MLImage2.h"

/// <summary>
/// PNGファイルを書き込む。
/// </summary>
/// <returns>0:成功。負の数:失敗。</returns>
int MLPngWrite(const wchar_t* pngFilePath, const MLImage2& img);

