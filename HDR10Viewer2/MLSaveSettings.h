#pragma once
#include <string>
#include "SimpleIni.h"

class MLSaveSettings {
public:
    MLSaveSettings(const char *settingsFileName);
    ~MLSaveSettings(void);

    void SaveStringA(const char* key, const char* name);
    void SaveInt(const char* key, int v);

    // 無いときはdefaultValueが戻る。
    int LoadInt(const char* key, int defaultValue);

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
