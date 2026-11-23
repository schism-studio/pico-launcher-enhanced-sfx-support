#include "common.h"
#include "JsonAppSettingsService.h"

JsonAppSettingsService::JsonAppSettingsService(const char* filePath)
    : _filePath(filePath)
{
    if (!_serializer.RecoverSfxSave(_filePath)) {
        LOG_ERROR("Settings recovery failed; backup not overwritten\n");
        return;
    }
    if (!_serializer.Deserialize(&_appSettings, _filePath))
        Save();
}
