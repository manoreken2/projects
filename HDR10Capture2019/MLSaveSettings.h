#pragma once
#include <string>
#include "SimpleIni.h"

class MLSaveSettings {
public:
    MLSaveSettings(const char *settingsFileName);
    ~MLSaveSettings(void);

    void SaveInt(const char* key, int v);

    // 無いときはdefaultValueが戻る。
    int LoadInt(const char* key, int defaultValue);

    void SaveDouble(const char* key, double v);

    // 無いときはdefaultValueが戻る。
    double LoadDouble(const char* key, double defaultValue);

    void SaveStringA(const char* key, const char* name);

    // 無いときは空文字列が戻る。
    const std::string LoadStringA(const char* key);

    /// <summary>
    /// 設定をすべてファイルに保存する。
    /// </summary>
    void WriteToFile(void);

private:
    std::string mSettingsFileNameA;
    CSimpleIniA mIni;
};
