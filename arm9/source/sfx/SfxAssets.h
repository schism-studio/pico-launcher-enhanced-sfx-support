#pragma once
#include "common.h"
#include "SfxClass.h"

namespace SfxAssets {
constexpr unsigned kCount = kSfxClassCount + 2;
constexpr unsigned kSleep = kSfxClassCount;
constexpr unsigned kWake = kSfxClassCount + 1;
// The one place that defines where banks live. Themes remain in /_pico/themes/<name>/sfx.
constexpr const char* kBankRoot = "/sfx";
// Customizable stock bank, shared by all selections beneath theme overrides.
constexpr const char* kDefaultBank = "Default";
struct Info { u32 bytes, padded, frames, format; };
struct Clip { const void* samples; u32 words, frames, format; };
struct Sources { char theme[64]; char pack[256]; };
const char* Name(unsigned id);
bool SafeFolder(const char* name);
bool IsDefault(const char* name);
void SetSources(Sources& sources, const char* theme, const char* pack);
bool Inspect(const char* path, unsigned id, Info& info);
// 0 = selected bank (skipped for Default), 1 = theme,
// 2 = /sfx/Default then embedded stock. Existing arena limits and ownership are unchanged.
bool LoadLayer(const Sources& sources, unsigned layer, unsigned id,
    void*& arena, u32 capacity, u32& used, Clip& clip);
bool Stock(unsigned id, Clip& clip);
}
