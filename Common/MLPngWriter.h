#pragma once

#include "MLImage.h"

/// <summary>
/// PNGファイルを書き込む。
/// </summary>
/// <returns>0:成功。負の数:失敗。</returns>
int MLPngWrite(const wchar_t* pngFilePath, const MLImage& img);

