#pragma once
#include "common.h"
// Separate priority lifecycle cues, not queued behind normal UI events.
// Load on IO, Tick on main; join IO before Stop on main.
namespace LidSfxService {
    void LoadTheme(const char* theme, const char* pack = "", bool enabled = true);
    bool CanReload();
    void Tick();
    void Stop();
    bool ReceiveAck(u32 data);
}
