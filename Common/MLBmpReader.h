#pragma once

#include "MLImage2.h"

/// <summary>
/// BMP�t�@�C����ǂށB
/// </summary>
/// <returns>0:�����B���̐�:���s�B1:BMP�t�@�C���ł͖��������B</returns>
int MLBmpRead(const wchar_t* filePath, MLImage2& img_return);