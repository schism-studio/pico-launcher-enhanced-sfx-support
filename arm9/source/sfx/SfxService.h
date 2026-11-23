#pragma once
#include "common.h"
#include "SfxClass.h"

// One owner per running process. Request() is main-thread only; LoadTheme()
// runs on that process's IO queue. Join the queue before Stop/destruction.
class SfxService
{
public:
    SfxService();
    ~SfxService();
    SfxService(const SfxService&) = delete;
    SfxService& operator=(const SfxService&) = delete;
    static void Request(SfxClass sound);
    void LoadTheme(const char* theme, const char* pack = "", bool enabled = true);
    static bool CanReload();
    void Tick();
    void Play(SfxClass sound);
    void PlayMove() { Play(SfxClass::Move); }
    void Flush();
    bool WantsLaunch() const { return _launchRequested; }
    void PlayLaunchAndWait();
    void Stop();
private:
    bool Send(SfxClass sound, bool exclusive = false);
    bool StopChannels();
    void* _state = nullptr;
    bool _ready = false;
    bool _enabled = true;
    bool _disabled = false;
    bool _launchRequested = false;
    SfxClass _request = SfxClass::None;
    u32 _frame = 0;
    unsigned _nextAction = 1;
};
