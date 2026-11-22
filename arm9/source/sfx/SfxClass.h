#pragma once
#include <stddef.h>

// Fixed semantic identifiers. Count is not a playable class.
enum class SfxClass : unsigned char
{
    Move,
    Confirm,
    Back,
    Launch,
    PageJump,
    ToggleOn,
    ToggleOff,
    SheetOpen,
    SheetClose,
    Favorite,
    Warning,
    Boundary,
    Count,
    None = 255
};

enum class SfxChannelMode : unsigned char
{
    InterruptSelf,
    ActionPool,
    Exclusive
};

struct SfxPolicy
{
    const char* filename;
    SfxClass fallback;
    SfxChannelMode channelMode;
    unsigned char cooldownFrames;
    unsigned char volume;
};

inline constexpr size_t kSfxClassCount =
    static_cast<size_t>(SfxClass::Count);

// Declarative target policy. Only Move is implemented in the current PoC.
// Filenames omit the extension: try valid .adpcm, then .pcm16.
// ActionPool reserves channels 3-5; InterruptSelf reserves channel 2.
// Warning's volume/fallback are provisional pending listening tests.
inline constexpr SfxPolicy kSfxPolicies[] =
{
    {"move",       SfxClass::None,    SfxChannelMode::InterruptSelf, 3,  80},
    {"confirm",    SfxClass::None,    SfxChannelMode::ActionPool,    0, 110},
    {"back",       SfxClass::None,    SfxChannelMode::ActionPool,    0, 105},
    {"launch",     SfxClass::Confirm, SfxChannelMode::Exclusive,     0, 127},
    {"pagejump",   SfxClass::Move,    SfxChannelMode::InterruptSelf, 3,  90},
    {"toggleon",   SfxClass::Confirm, SfxChannelMode::ActionPool,    0, 100},
    {"toggleoff",  SfxClass::Back,    SfxChannelMode::ActionPool,    0, 100},
    {"sheetopen",  SfxClass::Confirm, SfxChannelMode::ActionPool,    0,  95},
    {"sheetclose", SfxClass::Back,    SfxChannelMode::ActionPool,    0,  95},
    {"favorite",   SfxClass::Confirm, SfxChannelMode::ActionPool,    0, 110},
    {"warning",    SfxClass::Confirm, SfxChannelMode::ActionPool,    0, 110},
    {"boundary",   SfxClass::None,    SfxChannelMode::InterruptSelf, 4,  75}
};

static_assert(sizeof(kSfxPolicies) / sizeof(kSfxPolicies[0]) ==
              kSfxClassCount);

inline constexpr const SfxPolicy* GetSfxPolicy(SfxClass sound)
{
    const size_t index = static_cast<size_t>(sound);
    return index < kSfxClassCount ? &kSfxPolicies[index] : nullptr;
}

