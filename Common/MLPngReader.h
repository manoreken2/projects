#pragma once

#include "MLImage2.h"

/// <summary>
/// PNG�t�@�C����ǂށB
/// </summary>
/// <returns>0:�����B���̐�:���s�B1:PNG�t�@�C���ł͖��������B</returns>
int MLPngRead(const wchar_t* pngFilePath, MLImage2& img_return);

