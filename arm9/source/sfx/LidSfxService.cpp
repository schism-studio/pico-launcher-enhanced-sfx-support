#include "common.h"
#include "LidSfxService.h"
#include "SfxAssets.h"
#include "lidSfxConfig.h"
#include "soundIpcCommand.h"
#include "ipcChannels.h"
#include "fat/File.h"
#include "core/mini-printf.h"
#include "VBlank.h"
#include <nds/arm9/cache.h>
#include <libtwl/ipc/ipcFifoSystem.h>
#include <libtwl/rtos/rtosIrq.h>
#include <../../libtwl7/include/libtwl/sound/soundChannel.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

namespace {
constexpr u32 kRate = 22050;
constexpr u32 kBudget = 48 * 1024; // Separate from the 256 KiB UI arena.
constexpr u32 kTimeout = 60;
struct alignas(32) ConfigList { u32 count, command; const LidSfxConfig* config; };
struct alignas(32) State {
    ConfigList enable;
    ConfigList disable;
    LidSfxConfig clips;
    void* samples;
    u32 enableToken, disableToken;
};
State* state = nullptr;
volatile u32 receivedAck = 0;
u32 sequence = 0, elapsed = 0;
bool submitted = false, warned = false, quarantined = false;
u32 Token() {
    sequence = (sequence + 1) & kLidSfxTokenMask;
    if (!sequence) ++sequence;
    return sequence;
}

}

bool LidSfxService::ReceiveAck(u32 data) {
    if (!(data & kLidSfxAckFlag)) return false;
    receivedAck = data & kLidSfxTokenMask;
    return true;
}

bool LidSfxService::CanReload() { return !quarantined; }

void LidSfxService::LoadTheme(const char* theme, const char* pack, bool enabled) {
    if (!enabled || __atomic_load_n(&state, __ATOMIC_ACQUIRE) || quarantined) return;
    SfxAssets::Sources sources;
    SfxAssets::SetSources(sources, theme, pack);
    State* s = static_cast<State*>(memalign(32, sizeof(State)));
    if (!s) return;
    memset(s, 0, sizeof(State));
    u32 used = 0;
    for (unsigned layer = 0; layer < 3; ++layer) {
        for (unsigned which = 0; which < 2; ++which) {
            LidSfxClipConfig& target = which ? s->clips.wake : s->clips.sleep;
            if (target.samples) continue;
            SfxAssets::Clip clip;
            if (!SfxAssets::LoadLayer(sources, layer, which ? SfxAssets::kWake : SfxAssets::kSleep,
                s->samples, kBudget, used, clip)) continue;
            target.samples = clip.samples;
            target.timer = static_cast<u16>(-((33513982u + kRate) / (2u * kRate)));
            target.words = clip.words;
            target.control = SOUNDCNT_VOLUME(95) | SOUNDCNT_PAN(64) | SOUNDCNT_MODE_ONCE | clip.format;
            target.frames = clip.frames;
        }
    }
    if (!s->clips.sleep.samples && !s->clips.wake.samples) { free(s->samples); free(s); return; }
    s->enableToken = Token();
    s->disableToken = Token();
    s->enable.count = s->disable.count = 1;
    s->enable.command = (s->enableToken << 8) | SND_IPC_CMD_LID_SFX_CONFIG;
    s->disable.command = (s->disableToken << 8) | SND_IPC_CMD_LID_SFX_CONFIG;
    s->enable.config = &s->clips;
    s->disable.config = nullptr;
    if (used) DC_FlushRange(s->samples, used);
    DC_FlushRange(s, sizeof(State));
    submitted = warned = false;
    elapsed = 0;
    receivedAck = 0;
    __atomic_store_n(&state, s, __ATOMIC_RELEASE);
}

void LidSfxService::Tick() {
    State* s = __atomic_load_n(&state, __ATOMIC_ACQUIRE);
    if (!s || quarantined || warned) return;
    if (!submitted) submitted = ipc_trySendFifoMessage(IPC_CHANNEL_SOUND, reinterpret_cast<u32>(&s->enable));
    if (submitted && receivedAck == s->enableToken) return;
    if (++elapsed >= kTimeout) {
        warned = true;
        LOG_DEBUG("Lid SFX registration unconfirmed; sleep remains functional\n");
        // Keep immutable storage live. Never repeatedly reconfigure a playing cue.
    }
}

void LidSfxService::Stop() {
    State* s = __atomic_load_n(&state, __ATOMIC_ACQUIRE);
    if (!s) return;
    __atomic_store_n(&state, static_cast<State*>(nullptr), __ATOMIC_RELEASE);
    bool stopped = !submitted;
    if (submitted) {
        bool sent = false;
        receivedAck = 0;
        for (u32 frame = 0; frame < kTimeout; ++frame) {
            if (!sent) sent = ipc_trySendFifoMessage(IPC_CHANNEL_SOUND, reinterpret_cast<u32>(&s->disable));
            if (sent && receivedAck == s->disableToken) { stopped = true; break; }
            VBlank::Wait();
        }
    }
    if (stopped) {
        // Configure(nullptr) stopped channel 6 and discarded both pointers on ARM7.
        VBlank::Wait();
        free(s->samples);
        free(s);
    } else {
        quarantined = true; // At most one retained allocation; no repeated theme leaks.
        LOG_DEBUG("Lid SFX stop unconfirmed; retaining storage until restart\n");
    }
}
