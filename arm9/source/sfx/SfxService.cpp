#include "common.h"
#include "SfxService.h"
#include "LidSfxService.h"
#include "SfxAssets.h"
#include "fat/File.h"
#include "core/mini-printf.h"
#include "soundIpcCommand.h"
#include "ipcChannels.h"
#include "VBlank.h"
#include <nds/arm9/cache.h>
#include <libtwl/ipc/ipcFifoSystem.h>
#include <libtwl/rtos/rtosIrq.h>
#include <../../libtwl7/include/libtwl/sound/soundChannel.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

namespace {
constexpr u32 kBudget = 256 * 1024;
constexpr u32 kRate = 22050;
constexpr u32 kTimeoutFrames = 60;
constexpr u32 kSfxMask = 0x3C;
SfxService* activeService = nullptr;
bool transportFailed = false; // Quarantine on uncertain ARM7 ownership until reboot.
u32 nextToken = 0;
volatile u32 ack[4] = {};
u32 Token(unsigned slot) {
    nextToken = (nextToken + 1) & 0x3FFFFF;
    if (!nextToken) ++nextToken;
    return (nextToken << 2) | slot;
}
void ReceiveAck(u32 channel, u32 data, void* arg) {
    (void)channel; (void)arg;
    if (LidSfxService::ReceiveAck(data)) return;
    ack[data & 3] = data;
}
struct alignas(32) PlayList {
    u32 count, stop;
    snd_ipc_cmd_setup_channel_t setup;
    u32 start, acknowledge;
};
struct alignas(32) StopList { u32 count, stop, acknowledge; };
struct Clip {
    const void* samples;
    u32 length, format, frames;
    bool valid;
};
struct Channel {
    u32 token, sentFrame, busyUntil;
    bool pending;
};
struct alignas(32) State {
    // Immutable after publication: no command/sample mutation while ARM7 uses it.
    PlayList commands[kSfxClassCount][4];
    StopList stop;
    Clip clips[kSfxClassCount];
    Channel channels[4];
    u32 lastPlay[kSfxClassCount];
    bool played[kSfxClassCount];
    void* arena;
    u32 stopToken;
};
static_assert(sizeof(PlayList) % 32 == 0);
static_assert(sizeof(StopList) % 32 == 0);
unsigned Priority(SfxClass c) {
    switch (c) {
        case SfxClass::Launch: return 100;
        case SfxClass::Warning: return 90;
        case SfxClass::Favorite:
        case SfxClass::ToggleOn:
        case SfxClass::ToggleOff: return 80;
        case SfxClass::SheetOpen:
        case SfxClass::SheetClose: return 70;
        case SfxClass::Confirm:
        case SfxClass::Back: return 60;
        case SfxClass::PageJump: return 50;
        case SfxClass::Move: return 40;
        case SfxClass::Boundary: return 10;
        default: return 0;
    }
}
int Resolve(const State* s, SfxClass sound) {
    for (size_t depth = 0; depth < kSfxClassCount; ++depth) {
        const auto* policy = GetSfxPolicy(sound);
        if (!policy) return -1;
        size_t i = static_cast<size_t>(sound);
        if (s->clips[i].valid) return static_cast<int>(i);
        sound = policy->fallback;
    }
    return -1; // Bounded even if a future policy introduces a cycle.
}
}

SfxService::SfxService() { activeService = this; }
SfxService::~SfxService() {
    Stop();
    if (activeService == this) activeService = nullptr;
}
void SfxService::Request(SfxClass sound) {
    if (activeService) activeService->Play(sound);
}

bool SfxService::CanReload() { return !transportFailed && LidSfxService::CanReload(); }

void SfxService::LoadTheme(const char* theme, const char* pack, bool enabled)
{
    ipc_setChannelHandler(IPC_CHANNEL_SOUND, ReceiveAck, nullptr);
    __atomic_store_n(&_enabled, enabled, __ATOMIC_RELEASE);
    if (!enabled) return; // Previous owner stopped before this process was constructed.
    LidSfxService::LoadTheme(theme, pack, enabled);
    if (_state || transportFailed) return;
    SfxAssets::Sources sources;
    SfxAssets::SetSources(sources, theme, pack);
    State* s = static_cast<State*>(memalign(32, sizeof(State)));
    if (!s) return;
    memset(s, 0, sizeof(State));
    u32 used = 0;
    unsigned loaded = 0;
    // Reserve SD budget for all pack overrides before filling theme gaps.
    for (unsigned layer = 0; layer < 3; ++layer) {
        for (size_t i = 0; i < kSfxClassCount; ++i) {
            if (s->clips[i].valid) continue;
            SfxAssets::Clip clip;
            if (!SfxAssets::LoadLayer(sources, layer, i, s->arena, kBudget, used, clip)) continue;
            s->clips[i] = {clip.samples, clip.words, clip.format, clip.frames, true};
            ++loaded;
        }
    }
    if (!loaded) { free(s->arena); free(s); return; }
    // Build a separate immutable list for each requested class and channel.
    // Fallback changes the clip, not the originating action's channel policy.
    for (size_t i = 0; i < kSfxClassCount; ++i) {
        int resolved = Resolve(s, static_cast<SfxClass>(i));
        if (resolved < 0) continue;
        const Clip& clip = s->clips[resolved];
        for (unsigned ch = 0; ch < 4; ++ch) {
            PlayList& list = s->commands[i][ch];
            u32 token = Token(ch);
            list.count = 4;
            u32 mask = kSfxPolicies[i].channelMode == SfxChannelMode::Exclusive
                ? kSfxMask : 1u << (ch + 2);
            list.stop = (mask << 8) | SND_IPC_CMD_STOP_CHANNELS;
            list.setup.cmd = SND_IPC_CMD_SETUP_CHANNEL;
            list.setup.channel = ch + 2;
            list.setup.sourceAddress = clip.samples;
            list.setup.timer = static_cast<u16>(-((33513982u + kRate) / (kRate * 2u)));
            list.setup.loopStart = 0;
            list.setup.length = clip.length;
            list.setup.control = SOUNDCNT_VOLUME(kSfxPolicies[i].volume) |
                SOUNDCNT_PAN(64) | SOUNDCNT_MODE_ONCE | clip.format;
            list.start = ((1u << (ch + 2)) << 8) | SND_IPC_CMD_START_CHANNELS;
            list.acknowledge = (token << 8) | SND_IPC_CMD_ACKNOWLEDGE;
        }
    }
    s->stopToken = Token(0);
    s->stop.count = 2;
    s->stop.stop = (kSfxMask << 8) | SND_IPC_CMD_STOP_CHANNELS;
    s->stop.acknowledge = (s->stopToken << 8) | SND_IPC_CMD_ACKNOWLEDGE;
    ipc_setChannelHandler(IPC_CHANNEL_SOUND, ReceiveAck, nullptr);
    if (used) DC_FlushRange(s->arena, used);
    DC_FlushRange(s->commands, sizeof(s->commands));
    DC_FlushRange(&s->stop, sizeof(s->stop));
    _state = s;
    __atomic_store_n(&_ready, true, __ATOMIC_RELEASE);
    LOG_DEBUG("SFX: %u clips, %u bytes used (256 KiB arena)\n", loaded, used);
}

void SfxService::Tick()
{
    LidSfxService::Tick();
    ++_frame;
    if (!__atomic_load_n(&_ready, __ATOMIC_ACQUIRE) || _disabled) return;
    State* s = static_cast<State*>(_state);
    for (unsigned i = 0; i < 4; ++i) {
        Channel& channel = s->channels[i];
        if (!channel.pending) continue;
        if (ack[i] == channel.token) channel.pending = false;
        else if (_frame - channel.sentFrame >= kTimeoutFrames) {
            _disabled = true;
            LOG_DEBUG("SFX acknowledgement timeout; playback disabled\n");
        }
    }
}

void SfxService::Play(SfxClass sound)
{
    if (!__atomic_load_n(&_enabled, __ATOMIC_ACQUIRE) || !GetSfxPolicy(sound)) return;
    if (sound == SfxClass::Launch) {
        _launchRequested = true;
        _request = SfxClass::None;
        return;
    }
    if (_launchRequested) return;
    // Exactly one primary event per UI frame; specific actions beat navigation.
    // Do not turn a missing specific asset into a different incidental event.
    if (Priority(sound) > Priority(_request)) _request = sound;
}

void SfxService::Flush()
{
    SfxClass sound = _request;
    _request = SfxClass::None;
    if (sound != SfxClass::None && !_launchRequested) Send(sound);
}

bool SfxService::Send(SfxClass sound, bool exclusive)
{
    if (!__atomic_load_n(&_enabled, __ATOMIC_ACQUIRE) ||
        !__atomic_load_n(&_ready, __ATOMIC_ACQUIRE) || _disabled || transportFailed)
        return false;
    const auto* policy = GetSfxPolicy(sound);
    if (!policy || (policy->channelMode == SfxChannelMode::Exclusive && !exclusive)) return false;
    State* s = static_cast<State*>(_state);
    size_t index = static_cast<size_t>(sound);
    int resolved = Resolve(s, sound);
    if (resolved < 0) return false;
    if (s->played[index] && _frame - s->lastPlay[index] < policy->cooldownFrames)
        return false;
    unsigned ch = 0;
    if (policy->channelMode == SfxChannelMode::ActionPool) {
        // Free voice first, then steal the oldest acknowledged action voice.
        bool found = false;
        u32 oldestAge = 0;
        for (unsigned n = 0; n < 3; ++n) {
            unsigned candidate = 1 + ((_nextAction - 1 + n) % 3);
            const Channel& c = s->channels[candidate];
            if (c.pending) continue;
            if (!c.token || static_cast<s32>(_frame - c.busyUntil) >= 0) {
                ch = candidate; found = true; break;
            }
            u32 age = _frame - c.sentFrame;
            if (!found || age > oldestAge) { ch = candidate; oldestAge = age; found = true; }
        }
        if (!found) return false; // Never wait in a UI hook.
    }
    if (s->channels[ch].pending) return false;
    const PlayList& list = s->commands[index][ch];
    u32 irqs = rtos_disableIrqs();
    ack[ch] = 0;
    bool sent = ipc_trySendFifoMessage(IPC_CHANNEL_SOUND, reinterpret_cast<u32>(&list));
    rtos_restoreIrqs(irqs);
    if (!sent) return false;
    Channel& c = s->channels[ch];
    c.token = list.acknowledge >> 8;
    c.sentFrame = _frame;
    c.busyUntil = _frame + s->clips[resolved].frames;
    c.pending = true;
    s->played[index] = true;
    s->lastPlay[index] = _frame;
    if (ch) _nextAction = 1 + ch % 3;
    return true;
}

bool SfxService::StopChannels()
{
    State* s = static_cast<State*>(_state);
    bool sent = false, stopped = false;
    ack[0] = 0;
    for (u32 frame = 0; frame < kTimeoutFrames; ++frame) {
        if (!sent) sent = ipc_trySendFifoMessage(IPC_CHANNEL_SOUND, reinterpret_cast<u32>(&s->stop));
        if (sent && ack[0] == s->stopToken) { stopped = true; break; }
        VBlank::Wait();
    }
    if (!stopped) {
        transportFailed = true;
        _disabled = true;
        return false;
    }
    VBlank::Wait();
    memset(s->channels, 0, sizeof(s->channels));
    return true;
}

void SfxService::PlayLaunchAndWait()
{
    // Caller has joined its IO work and stopped BGM, but still owns SFX RAM.
    if (!_launchRequested || !__atomic_load_n(&_ready, __ATOMIC_ACQUIRE) || _disabled) return;
    State* s = static_cast<State*>(_state);
    int resolved = Resolve(s, SfxClass::Launch);
    if (resolved < 0 || !StopChannels()) return;
    bool sent = false;
    for (u32 retry = 0; retry < kTimeoutFrames && !sent; ++retry) {
        sent = Send(SfxClass::Launch, true);
        if (!sent) { VBlank::Wait(); Tick(); }
    }
    if (!sent) return;
    u32 frames = s->clips[resolved].frames;
    for (u32 frame = 0; frame < frames; ++frame) { VBlank::Wait(); Tick(); }
}

void SfxService::Stop()
{
    __atomic_store_n(&_enabled, false, __ATOMIC_RELEASE);
    LidSfxService::Stop();
    _request = SfxClass::None;
    if (!__atomic_load_n(&_ready, __ATOMIC_ACQUIRE)) return;
    __atomic_store_n(&_ready, false, __ATOMIC_RELEASE);
    State* s = static_cast<State*>(_state);
    if (StopChannels()) {
        free(s->arena);
        free(s);
    }
    // A failed stop intentionally retains RAM and disables future SFX loads.
    _state = nullptr;
    _disabled = true;
}
