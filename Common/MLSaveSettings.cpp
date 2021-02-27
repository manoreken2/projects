#include "MLSaveSettings.h"
#include <Windows.h>
#include "DXSampleHelper.h"
#include <shlwapi.h>

static const char* gSectionName = "Main";

static void
GetAssetsPathA(_Out_writes_(pathSize) char* path, UINT pathSize)
{
    if (path == nullptr) {
        throw std::exception();
    }

    DWORD size = GetModuleFileNameA(nullptr, path, pathSize);
    if (size == 0 || size == pathSize) {
        // Method failed or path was truncated.
        throw std::exception();
    }

    char* lastSlash = strrchr(path, L'\\');
    if (lastSlash) {
        *(lastSlash + 1) = L'\0';
    }
}

MLSaveSettings::MLSaveSettings(const char *settingsFileName)
{
    char path[MAX_PATH];
    GetAssetsPathA(path, _countof(path));

    mSettingsFileNameA = std::string(path) + settingsFileName;

    mIni.SetUnicode();
    mIni.LoadFile(mSettingsFileNameA.c_str());

    wchar_t nameW[256] = {};
}

MLSaveSettings::~MLSaveSettings(void) {
    
}

void
MLSaveSettings::WriteToFile(void) {
    mIni.SaveFile(mSettingsFileNameA.c_str());
}

void
MLSaveSettings::SaveInt(const char* key, int v) {
    mIni.SetLongValue(gSectionName, key, v);
}

int
MLSaveSettings::LoadInt(const char* key, int defaultValue) {
    return mIni.GetLongValue(gSectionName, key, defaultValue);
}

void
MLSaveSettings::SaveDouble(const char* key, double v) {
    mIni.SetDoubleValue(gSectionName, key, v);
}

double
MLSaveSettings::LoadDouble(const char* key, double defaultValue) {
    return mIni.GetDoubleValue(gSectionName, key, defaultValue);
}

void
MLSaveSettings::SaveStringA(const char* key, const char* name) {
    mIni.SetValue(gSectionName, key, name);
}

const std::string
MLSaveSettings::LoadStringA(const char* key) {
    const char *name_r = mIni.GetValue(gSectionName, key, "");
    if (nullptr == name_r || strlen(name_r) == 0) {
        return "";
    }

    // std::stringÇçÏÇ¡ÇƒñﬂÇ∑ÅB
    return name_r;
}

