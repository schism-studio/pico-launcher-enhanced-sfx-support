#pragma once
#include "common.h"
#include "SfxClass.h"

// App-owned; hardware-visible storage is allocated separately.
class SfxService
{
public:
    SfxService() = default;
    ~SfxService();
    void LoadTheme(const char* theme);
    void Tick();
    // Common UI entry point.
    // Reserved classes remain silent until their clips/channel policies
    // are implemented. They do NOT play the Move sound as a placeholder.
    void Play(SfxClass sound)
    {
        switch (sound)
        {
            case SfxClass::Move:
                PlayMove();
                break;
            default:
                break;
        }
    }

    void PlayMove();
    void Stop();
private:
    void* _state = nullptr;
    bool _ready = false;
    bool _disabled = false;
    bool _pending = false;
    u32 _frame = 0;
    u32 _lastPlay = 0;
    u32 _sentFrame = 0;
    bool _played = false;
};

