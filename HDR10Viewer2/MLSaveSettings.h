#pragma once
#include <string>
#include "SimpleIni.h"

class MLSaveSettings {
public:
    MLSaveSettings(const char *settingsFileName);
    ~MLSaveSettings(void);

    void SaveStringA(const char* key, const char* name);
    void SaveInt(const char* key, int v);

    // �����Ƃ���defaultValue���߂�B
    int LoadInt(const char* key, int defaultValue);

    // �����Ƃ��͋󕶎��񂪖߂�B
    const std::string LoadStringA(const char* key);

    /// <summary>
    /// �ݒ�����ׂăt�@�C���ɕۑ�����B
    /// </summary>
    void WriteToFile(void);

private:
    std::string mSettingsFileNameA;
    CSimpleIniA mIni;
};
