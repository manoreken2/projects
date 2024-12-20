#pragma once

#include "MLImage2.h"

/// <summary>
/// 48bit BMPファイルを書き込む。
/// </summary>
/// <returns>0:成功。負の数:失敗。</returns>
int MLBmpWrite(const wchar_t* filePath, MLImage2& img);

