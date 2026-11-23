#include "common.h"
#include <memory>
#include "json/ArduinoJson.h"
#include "AppSettings.h"
#include "fat/File.h"
#include "JsonAppSettingsSerializer.h"
#include "IAppSettingsService.h"
#include "sfx/SfxAssets.h"
#include "core/mini-printf.h"
#include <stdlib.h>

#pragma GCC optimize("Os")

#define JSON_RESERVED_SIZE  4096

#define KEY_LANGUAGE                 "language"
#define KEY_ROM_BROWSER_LAYOUT       "romBrowserLayout"
#define KEY_ROM_BROWSER_SORT_MODE    "romBrowserSortMode"
#define KEY_ROM_BROWSER_HIDE_EMPTY_FOLDERS  "romBrowserHideEmptyFolders"
#define KEY_THEME                    "theme"
#define KEY_SFX_ENABLED              "sfxEnabled"
#define KEY_SFX_PACK                 "sfxPack"
#define KEY_LAST_USED_FILE_PATH      "lastUsedFilePath"
#define KEY_BACKLIGHT_LEVEL          "backlightLevel"
#define KEY_SAVE_LOCATION            "saveLocation"
#define KEY_LAUNCH_TRACKING          "launchTracking"
#define KEY_FILE_ASSOCIATIONS        "fileAssociations"
#define KEY_FILE_ASSOCIATIONS_APPLICATION_PATH  "appPath"

static const char* serializeRomBrowserLayout(RomBrowserLayout romBrowserLayout)
{
    switch (romBrowserLayout)
    {
        case RomBrowserLayout::HorizontalIconGrid:
            return "HorizontalIconGrid";
        case RomBrowserLayout::VerticalIconGrid:
            return "VerticalIconGrid";
        case RomBrowserLayout::BannerList:
            return "BannerList";
        case RomBrowserLayout::FileList:
            return "FileList";
        case RomBrowserLayout::CoverFlow:
            return "CoverFlow";
        default:
            return "";
    }
}

static bool tryParseRomBrowserLayout(
    const char* romBrowserLayoutString, RomBrowserLayout& romBrowserLayout)
{
    if (!romBrowserLayoutString)
        return false;

    if (!strcasecmp(romBrowserLayoutString, "HorizontalIconGrid"))
        romBrowserLayout = RomBrowserLayout::HorizontalIconGrid;
    else if (!strcasecmp(romBrowserLayoutString, "VerticalIconGrid"))
        romBrowserLayout = RomBrowserLayout::VerticalIconGrid;
    else if (!strcasecmp(romBrowserLayoutString, "BannerList"))
        romBrowserLayout = RomBrowserLayout::BannerList;
    else if (!strcasecmp(romBrowserLayoutString, "FileList"))
        romBrowserLayout = RomBrowserLayout::FileList;
    else if (!strcasecmp(romBrowserLayoutString, "CoverFlow"))
        romBrowserLayout = RomBrowserLayout::CoverFlow;
    else
        return false;

    return true;
}

static const char* serializeRomBrowserSortMode(RomBrowserSortMode romBrowserSortMode)
{
    switch (romBrowserSortMode)
    {
        case RomBrowserSortMode::NameAscending:
            return "NameAscending";
        case RomBrowserSortMode::NameDescending:
            return "NameDescending";
        case RomBrowserSortMode::LastModified:
            return "LastModified";
        default:
            return "";
    }
}

static bool tryParseRomBrowserSortMode(
    const char* romBrowserDisplayModeString, RomBrowserSortMode& romBrowserSortMode)
{
    if (!romBrowserDisplayModeString)
        return false;

    if (!strcasecmp(romBrowserDisplayModeString, "NameAscending"))
        romBrowserSortMode = RomBrowserSortMode::NameAscending;
    else if (!strcasecmp(romBrowserDisplayModeString, "NameDescending"))
        romBrowserSortMode = RomBrowserSortMode::NameDescending;
    else if (!strcasecmp(romBrowserDisplayModeString, "LastModified"))
        romBrowserSortMode = RomBrowserSortMode::LastModified;
    else
        return false;

    return true;
}

static const char* serializeSaveLocation(SaveLocation saveLocation)
{
    switch (saveLocation)
    {
        case SaveLocation::NextToRom:
            return "rom";
        case SaveLocation::SavesFolder:
            return "saves";
        default:
            return "";
    }
}

static bool tryParseSaveLocation(const char* saveLocationString, SaveLocation& saveLocation)
{
    if (!saveLocationString)
        return false;

    if (!strcasecmp(saveLocationString, "rom"))
        saveLocation = SaveLocation::NextToRom;
    else if (!strcasecmp(saveLocationString, "saves"))
        saveLocation = SaveLocation::SavesFolder;
    else
        return false;

    return true;
}

static bool tryParseFileAssociations(const JsonObjectConst& json, AppSettings* appSettings)
{
    if (json.isNull())
    {
        return false;
    }

    appSettings->fileAssociations = std::make_unique_for_overwrite<FileAssociation[]>(json.size());
    int i = 0;
    for (auto item : json)
    {
        auto extension = item.key().c_str();
        auto appPath = item.value()[KEY_FILE_ASSOCIATIONS_APPLICATION_PATH].as<const char*>();
        appSettings->fileAssociations[i++] = FileAssociation(extension, appPath);
    }
    appSettings->numberOfFileAssociations = i;
    return true;
}

static void serializeFileAssociations(DynamicJsonDocument& json, const AppSettings* appSettings)
{
    auto jsonObject = json[KEY_FILE_ASSOCIATIONS].to<JsonObject>();
    for (u32 i = 0; i < appSettings->numberOfFileAssociations; i++)
    {
        const auto& fileAssociation = appSettings->fileAssociations[i];
        auto jsonAssociation = jsonObject[fileAssociation.extension.GetString()].to<JsonObject>();
        jsonAssociation[KEY_FILE_ASSOCIATIONS_APPLICATION_PATH] = fileAssociation.applicationPath.GetString();
    }
}

static std::unique_ptr<u8[]> writeJson(const AppSettings* appSettings, u32& length, const bool* enabled = nullptr, const char* bank = nullptr)
{
    DynamicJsonDocument json(JSON_RESERVED_SIZE);
    json[KEY_LANGUAGE] = appSettings->language.GetString();
    json[KEY_ROM_BROWSER_LAYOUT] = serializeRomBrowserLayout(appSettings->romBrowserDisplaySettings.layout);
    json[KEY_ROM_BROWSER_SORT_MODE] = serializeRomBrowserSortMode(appSettings->romBrowserDisplaySettings.sortMode);
    json[KEY_ROM_BROWSER_HIDE_EMPTY_FOLDERS] = appSettings->romBrowserDisplaySettings.hideEmptyFolders;
    json[KEY_THEME] = appSettings->theme.GetString();
    json[KEY_SFX_ENABLED] = enabled ? *enabled : appSettings->sfxEnabled;
    json[KEY_SFX_PACK] = bank ? bank : appSettings->sfxPack.GetString();
    json[KEY_LAST_USED_FILE_PATH] = appSettings->lastUsedFilePath.GetString();
    // only written once the user picked a level; -1 keeps the firmware's
    if (appSettings->backlightLevel >= 0)
        json[KEY_BACKLIGHT_LEVEL] = appSettings->backlightLevel;
    json[KEY_SAVE_LOCATION] = serializeSaveLocation(appSettings->saveLocation);
    json[KEY_LAUNCH_TRACKING] = appSettings->launchTracking;
    serializeFileAssociations(json, appSettings);

    if (json.overflowed()) {
        LOG_ERROR("Settings document full; original settings file left untouched\n");
        return nullptr;
    }
    u32 outputSize = measureJsonPretty(json);
    std::unique_ptr<u8[]> fileData(new(cache_align) u8[outputSize]);

    serializeJsonPretty(json, fileData.get(), outputSize);

    length = outputSize;
    return fileData;
}

void JsonAppSettingsSerializer::Serialize(const AppSettings* appSettings, const char* filePath) const
{
    u32 length = 0;
    std::unique_ptr<u8[]> fileData = writeJson(appSettings, length);
    if (!fileData) return;

    const auto file = std::make_unique<File>();
    if (file->Open(filePath, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
    {
        LOG_ERROR("Couldn't open settings file for writing\n");
        return;
    }

    u32 bytesWritten;
    if (file->Write(fileData.get(), length, bytesWritten) != FR_OK || bytesWritten != length)
    {
        LOG_ERROR("Error while writing settings file\n");
        return;
    }

    LOG_DEBUG("Settings file written\n");
}


namespace {
struct SavePaths { char temp[320], backup[320]; FILINFO info; };
bool Paths(SavePaths& p,const char* path) {
    int a=mini_snprintf(p.temp,sizeof(p.temp),"%s.sfx-tmp",path);
    int b=mini_snprintf(p.backup,sizeof(p.backup),"%s.sfx-bak",path);
    return a>0 && (unsigned)a<sizeof(p.temp) && b>0 && (unsigned)b<sizeof(p.backup);
}
bool Missing(FRESULT r) { return r==FR_NO_FILE; }
}
bool JsonAppSettingsSerializer::RecoverSfxSave(const char* path) const {
    std::unique_ptr<SavePaths,decltype(&free)> p(static_cast<SavePaths*>(calloc(1,sizeof(SavePaths))),&free);
    if (!p || !Paths(*p,path)) return false;
    FRESULT r=f_stat(path,&p->info);
    if (r==FR_NO_PATH) return true;
    if (r==FR_OK) {
        if (p->info.fattrib&AM_DIR) return false;
        FRESULT backup=f_stat(p->backup,&p->info);
        if (backup==FR_OK) {
            // Never discard the previous file until the committed JSON can be read back.
            AppSettings verified;
            if ((p->info.fattrib&AM_DIR) || !Deserialize(&verified,path)) return false;
            f_unlink(p->backup); // A failed cleanup is reported by the next save's preflight.
        } else if (!Missing(backup)) return false;
        return true;
    }
    if (!Missing(r)) return false;
    r=f_stat(p->backup,&p->info);
    if (Missing(r)) return true;
    return r==FR_OK && !(p->info.fattrib&AM_DIR) && f_rename(p->backup,path)==FR_OK;
}
SfxSaveResult JsonAppSettingsSerializer::SerializeSfx(const AppSettings* settings,const char* path,
    bool enabled,const char* bank) const {
    if (!bank || (!SfxAssets::IsDefault(bank) && !SfxAssets::SafeFolder(bank))) return SfxSaveResult::InvalidName;
    if (SfxAssets::IsDefault(bank)) bank="default";
    std::unique_ptr<SavePaths,decltype(&free)> p(static_cast<SavePaths*>(calloc(1,sizeof(SavePaths))),&free);
    if (!p || !Paths(*p,path)) return SfxSaveResult::Memory;
    if (!RecoverSfxSave(path)) return SfxSaveResult::Recovery;
    FRESULT r=f_stat(p->backup,&p->info);
    if (r==FR_OK) return SfxSaveResult::Recovery;
    if (!Missing(r)) return SfxSaveResult::Read;
    r=f_stat(p->temp,&p->info);
    if (r==FR_OK && f_unlink(p->temp)!=FR_OK) return SfxSaveResult::Open;
    if (r!=FR_OK && !Missing(r)) return SfxSaveResult::Read;
    r=f_stat(path,&p->info); bool original=r==FR_OK;
    if ((!original && !Missing(r)) || (original && (p->info.fattrib&AM_DIR))) return SfxSaveResult::Read;
    u32 length=0; auto bytes=writeJson(settings,length,&enabled,bank);
    if (!bytes) return SfxSaveResult::Memory;
    SfxSaveResult written=SfxSaveResult::Ok;
    {
        auto file=std::make_unique<File>();
        if (file->Open(p->temp,FA_WRITE|FA_CREATE_NEW)!=FR_OK) return SfxSaveResult::Open;
        u32 count=0;
        if (file->Write(bytes.get(),length,count)!=FR_OK || count!=length) written=SfxSaveResult::Write;
        else if (file->Sync()!=FR_OK) written=SfxSaveResult::Sync;
        if (file->Close()!=FR_OK && written==SfxSaveResult::Ok) written=SfxSaveResult::Close;
    }
    if (written!=SfxSaveResult::Ok) { f_unlink(p->temp); return written; }
    if (original && f_rename(path,p->backup)!=FR_OK) { f_unlink(p->temp); return SfxSaveResult::Backup; }
    if (f_rename(p->temp,path)!=FR_OK) {
        bool restored=!original || f_rename(p->backup,path)==FR_OK;
        if (restored) f_unlink(p->temp);
        return restored ? SfxSaveResult::Commit : SfxSaveResult::Recovery;
    }
    if (original) f_unlink(p->backup);
    return SfxSaveResult::Ok;
}

static void readJson(AppSettings* appSettings, const JsonDocument& json)
{
    appSettings->language = json[KEY_LANGUAGE] | appSettings->language.GetString();
    appSettings->theme = json[KEY_THEME] | appSettings->theme.GetString();
    appSettings->sfxEnabled = json[KEY_SFX_ENABLED] | true;
    appSettings->sfxPack = json[KEY_SFX_PACK] | "default";
    if (SfxAssets::IsDefault(appSettings->sfxPack.GetString()) || !SfxAssets::SafeFolder(appSettings->sfxPack.GetString()))
        appSettings->sfxPack = "default";
    appSettings->lastUsedFilePath = json[KEY_LAST_USED_FILE_PATH] | appSettings->lastUsedFilePath.GetString();

    int backlightLevel = json[KEY_BACKLIGHT_LEVEL] | -1;
    if (backlightLevel >= 0 && backlightLevel <= 3)
        appSettings->backlightLevel = (s8)backlightLevel;

    RomBrowserLayout romBrowserLayout;
    if (tryParseRomBrowserLayout(json[KEY_ROM_BROWSER_LAYOUT].as<const char*>(),
            romBrowserLayout))
    {
        appSettings->romBrowserDisplaySettings.layout = romBrowserLayout;
    }
    RomBrowserSortMode romBrowserSortMode;
    if (tryParseRomBrowserSortMode(json[KEY_ROM_BROWSER_SORT_MODE].as<const char*>(),
            romBrowserSortMode))
    {
        appSettings->romBrowserDisplaySettings.sortMode = romBrowserSortMode;
    }
    appSettings->romBrowserDisplaySettings.hideEmptyFolders =
        json[KEY_ROM_BROWSER_HIDE_EMPTY_FOLDERS] | appSettings->romBrowserDisplaySettings.hideEmptyFolders;

    SaveLocation saveLocation;
    if (tryParseSaveLocation(json[KEY_SAVE_LOCATION].as<const char*>(), saveLocation))
    {
        appSettings->saveLocation = saveLocation;
    }
    appSettings->launchTracking = json[KEY_LAUNCH_TRACKING] | appSettings->launchTracking;

    tryParseFileAssociations(json[KEY_FILE_ASSOCIATIONS], appSettings);
}

bool JsonAppSettingsSerializer::Deserialize(AppSettings* appSettings, const char* filePath) const
{
    const auto file = std::make_unique<File>();
    if (file->Open(filePath, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return false;

    u32 fileSize = file->GetSize();
    if (fileSize == 0)
        return false;

    std::unique_ptr<u8[]> fileData(new(cache_align) u8[fileSize]);
    u8* fileDataPtr = fileData.get();

    u32 bytesRead = 0;
    if (file->Read(fileDataPtr, fileSize, bytesRead) != FR_OK)
        return false;

    DynamicJsonDocument json(JSON_RESERVED_SIZE);
    if (deserializeJson(json, fileDataPtr, fileSize) != DeserializationError::Ok)
        return false;

    readJson(appSettings, json);

    return true;
}