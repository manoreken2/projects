#pragma once

#include "MLImage2.h"

/// <summary>
/// BMPファイルを読む。
/// </summary>
/// <returns>0:成功。負の数:失敗。1:BMPファイルでは無かった。</returns>
int MLBmpRead(const wchar_t* filePath, MLImage2& img_return);