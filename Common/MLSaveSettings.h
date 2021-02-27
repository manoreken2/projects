#pragma once
#include <string>
#include "SimpleIni.h"

class MLSaveSettings {
public:
    MLSaveSettings(const char *settingsFileName);
    ~MLSaveSettings(void);

    void SaveInt(const char* key, int v);

    // �����Ƃ���defaultValue���߂�B
    int LoadInt(const char* key, int defaultValue);

    void SaveDouble(const char* key, double v);

    // �����Ƃ���defaultValue���߂�B
    double LoadDouble(const char* key, double defaultValue);

    void SaveStringA(const char* key, const char* name);

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
