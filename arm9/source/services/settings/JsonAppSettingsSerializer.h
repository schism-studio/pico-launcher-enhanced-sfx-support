#pragma once
class AppSettings;
enum class SfxSaveResult;

class JsonAppSettingsSerializer
{
public:
    void Serialize(const AppSettings* appSettings, const char* filePath) const;
    bool RecoverSfxSave(const char* path) const;
    SfxSaveResult SerializeSfx(const AppSettings* settings, const char* path, bool enabled, const char* bank) const;
    bool Deserialize(AppSettings* appSettings, const char* filePath) const;
};