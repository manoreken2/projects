#pragma once

#include "MLImage.h"

/// <summary>
/// 48bit BMPファイルを書き込む。
/// </summary>
/// <returns>0:成功。負の数:失敗。</returns>
int MLBmpWrite(const char* filePath, MLImage& img);

