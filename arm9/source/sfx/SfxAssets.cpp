#include "common.h"
#include "SfxAssets.h"
#include "fat/File.h"
#include "core/mini-printf.h"
#include <nds/arm9/cache.h>
#include <../../libtwl7/include/libtwl/sound/soundChannel.h>
#include <malloc.h>
#include <string.h>

namespace {
constexpr u32 kRate = 22050;
bool InspectOpen(File& file, unsigned id, bool adpcm, SfxAssets::Info& info) {
    if (id >= SfxAssets::kCount) return false;
    auto size = file.GetSize();
    if (!size || size > 256 * 1024 || (!adpcm && (size & 1)) || (adpcm && size < 8)) return false;
    u32 bytes = static_cast<u32>(size), padded = (bytes + 3) & ~3u;
    if (padded < 16) padded = 16;
    u32 samples = adpcm ? 1 + (padded - 4) * 2 : padded / 2;
    u32 duration = (samples * 60 + kRate - 1) / kRate;
    if (duration > (id >= SfxAssets::kSleep ? 24u : 120u)) return false;
    if (adpcm) {
        u8 header[4];
        if (!file.ReadExact(header, sizeof(header)) || header[2] > 88 || header[3]) return false;
    }
    info = {bytes, padded, duration + 1,
        static_cast<u32>(adpcm ? SOUNDCNT_FORMAT_ADPCM : SOUNDCNT_FORMAT_PCM16)};
    return true;
}
}
const char* SfxAssets::Name(unsigned id) {
    if (id < kSfxClassCount) return kSfxPolicies[id].filename;
    return id == kSleep ? "sleep" : id == kWake ? "wake" : nullptr;
}
bool SfxAssets::SafeFolder(const char* name) {
    if (!name || !*name || strlen(name) > 255 || !strcmp(name, ".") || !strcmp(name, "..")) return false;
    size_t length = strlen(name);
    if (name[length-1] == '.' || name[length-1] == ' ') return false;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(name); *p; ++p)
        if (*p < 32 || *p == 127 || strchr("\\/:*?\"<>|", *p)) return false;
    return true;
}
bool SfxAssets::IsDefault(const char* name) {
    return !name || !*name || !strcasecmp(name, "default");
}
void SfxAssets::SetSources(Sources& sources, const char* theme, const char* pack) {
    memset(&sources, 0, sizeof(sources));
    if (SafeFolder(theme) && strlen(theme) < sizeof(sources.theme)) strlcpy(sources.theme, theme, sizeof(sources.theme));
    if (!IsDefault(pack) && SafeFolder(pack)) strlcpy(sources.pack, pack, sizeof(sources.pack));
}
bool SfxAssets::Inspect(const char* path, unsigned id, Info& info) {
    const char* extension = strrchr(path, '.');
    bool adpcm = extension && !strcasecmp(extension, ".adpcm");
    if (!adpcm && (!extension || strcasecmp(extension, ".pcm16"))) return false;
    File* file = new File;
    if (!file) return false;
    bool valid = file->Open(path, FA_READ | FA_OPEN_EXISTING) == FR_OK && InspectOpen(*file, id, adpcm, info);
    delete file;
    return valid;
}
bool SfxAssets::LoadLayer(const Sources& sources, unsigned layer, unsigned id,
    void*& arena, u32 capacity, u32& used, Clip& clip) {
    if (id >= kCount || layer > 2) return false;
    // The stock pass first tries the user-editable Default folder. Keep this in the
    // existing pass so higher-priority bank/theme clips retain first use of the arena.
    const char* folder = layer == 2 ? kDefaultBank : layer == 0 ? sources.pack : sources.theme;
    if (!SafeFolder(folder)) return false;
    for (unsigned format = 0; format < 2; ++format) {
        char path[512];
        int length = layer != 1
            ? mini_snprintf(path, sizeof(path), "%s/%s/%s.%s", kBankRoot, folder, Name(id), format ? "pcm16" : "adpcm")
            : mini_snprintf(path, sizeof(path), "/_pico/themes/%s/sfx/%s.%s", folder, Name(id), format ? "pcm16" : "adpcm");
        if (length < 0 || static_cast<unsigned>(length) >= sizeof(path)) continue;
        File* file = new File;
        if (!file) continue;
        Info info;
        bool valid = file->Open(path, FA_READ | FA_OPEN_EXISTING) == FR_OK &&
            InspectOpen(*file, id, format == 0, info);
        if (!valid) { delete file; continue; }
        if (used > capacity || info.padded > capacity - used) {
            LOG_DEBUG("SFX memory budget excludes %s\n", path);
            delete file; continue;
        }
        if (!arena) arena = memalign(32, capacity);
        if (!arena) { delete file; continue; }
        u8* dest = static_cast<u8*>(arena) + used;
        memset(dest, 0, info.padded);
        valid = file->Seek(0) == FR_OK && file->ReadExact(dest, info.bytes);
        delete file;
        if (!valid) continue;
        clip = {dest, info.padded >> 2, info.frames, info.format};
        used += info.padded;
        return true;
    }
    if (layer == 2 && Stock(id, clip)) {
        DC_FlushRange(clip.samples, clip.words * 4);
        return true;
    }
    return false;
}
