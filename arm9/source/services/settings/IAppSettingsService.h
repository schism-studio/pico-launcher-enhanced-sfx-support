#pragma once
#include "AppSettings.h"

enum class SfxSaveResult { Ok, Unsupported, Memory, InvalidName, Read, Open, Write, Sync, Close, Backup, Commit, Recovery };
class IAppSettingsService
{
public:
    virtual ~IAppSettingsService() { }

    virtual AppSettings& GetAppSettings() = 0;
    virtual const AppSettings& GetAppSettings() const = 0;
    virtual void Save() const = 0;
    // Persists a proposed SFX choice without mutating the live AppSettings object.
    virtual SfxSaveResult SaveSfx(bool enabled, const char* bank) const {
        (void)enabled; (void)bank; return SfxSaveResult::Unsupported;
    }
};