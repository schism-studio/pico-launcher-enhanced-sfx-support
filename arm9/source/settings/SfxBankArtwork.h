#pragma once
#include "common.h"
class MaterialColorScheme;
// Worker-owned CPU staging data, published only after its task completes.
struct alignas(32) SfxBankArtwork {
    alignas(32) u16 square[128*128];
    alignas(32) u8 icon[512];
    alignas(32) u16 palette[16];
    void Default(const MaterialColorScheme& colors);
    void Off(const MaterialColorScheme& colors);
    void Load(const char* bank,const char* theme,const MaterialColorScheme& colors,const bool* cancel);
    static void Speaker(u8* tiles,u16* palette,const MaterialColorScheme& colors);
    static void MutedSpeaker(u8* tiles,u16* palette,const MaterialColorScheme& colors);
private:
    void Package(const MaterialColorScheme& colors);
    void PaintIcon();
    bool ReadIcon(const char* path);
    bool ReadPreview(const char* path,const bool* cancel);
};
