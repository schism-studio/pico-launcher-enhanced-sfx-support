#include "common.h"
#include <libtwl/sound/soundChannel.h>
#include <libtwl/rtos/rtosIrq.h>
#include "lidSfx7.h"

namespace {
LidSfxConfig config = {};
bool gated = false;
u32 Play(const LidSfxClipConfig& clip) {
    snd_stopChannel(kLidSfxChannel);
    if (!clip.samples || !clip.words || !clip.frames ||
        clip.frames > kLidSfxMaxSampleFrames + 1) return 0;
    REG_SOUNDxSAD(kLidSfxChannel) = reinterpret_cast<u32>(clip.samples);
    REG_SOUNDxTMR(kLidSfxChannel) = clip.timer;
    REG_SOUNDxPNT(kLidSfxChannel) = 0;
    REG_SOUNDxLEN(kLidSfxChannel) = clip.words;
    REG_SOUNDxCNT(kLidSfxChannel) = clip.control;
    snd_startChannel(kLidSfxChannel);
    return clip.frames;
}
}

void lidSfxConfigure(const LidSfxConfig* next) {
    // Called by sound IPC. Stop channel 6 BEFORE replacing any sample pointers.
    // No retained descriptor pointer: ARM7 takes a value copy.
    u32 irqs = rtos_disableIrqs();
    snd_stopChannel(kLidSfxChannel);
    config = next ? *next : LidSfxConfig{};
    rtos_restoreIrqs(irqs);
}
u32 lidSfxBeginSleep() {
    u32 irqs = rtos_disableIrqs();
    gated = true;
    u32 frames = Play(config.sleep);
    rtos_restoreIrqs(irqs);
    return frames;
}
void lidSfxCancelSleep() {
    u32 irqs = rtos_disableIrqs();
    snd_stopChannel(kLidSfxChannel);
    gated = false;
    rtos_restoreIrqs(irqs);
}
void lidSfxWake() {
    u32 irqs = rtos_disableIrqs();
    gated = false;
    Play(config.wake);
    rtos_restoreIrqs(irqs);
}
void lidSfxStop() { snd_stopChannel(kLidSfxChannel); }
bool lidSfxAllowUi() { return !gated; }
