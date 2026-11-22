#include "common.h"
#include "SfxService.h"
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
volatile u32 receivedAck = 0;
u32 nextToken = 0;
u32 Token() { nextToken = (nextToken + 1) & 0xFFFFFF; if (!nextToken) ++nextToken; return nextToken; }
void ReceiveAck(u32 channel, u32 data, void* arg) { receivedAck = data; }
struct alignas(32) PlayList {
    u32 count;
    u32 stop;
    snd_ipc_cmd_setup_channel_t setup;
    u32 start;
    u32 ack;
};
struct alignas(32) StopList { u32 count, stop, ack; };
struct alignas(32) State {
    PlayList play;
    StopList stop;
    void* samples;
    u32 playToken, stopToken;
};
constexpr u32 kBudget = 256 * 1024;
constexpr u32 kMask = 1u << 2;
constexpr u32 kTimeoutFrames = 60;
}

SfxService::~SfxService() { Stop(); }

void SfxService::LoadTheme(const char* theme)
{
    // Called once by App's IO task; published only after construction.
    if (_state || !theme || !theme[0]) return;
    ipc_setChannelHandler(IPC_CHANNEL_SOUND, ReceiveAck, nullptr);
    State* s = static_cast<State*>(memalign(32, sizeof(State)));
    if (!s) return;
    memset(s, 0, sizeof(State));
    bool loaded = false;
    for (int format = 0; format < 2 && !loaded; ++format) {
        char path[256];
        int n = mini_snprintf(path, sizeof(path), "/_pico/themes/%s/sfx/move.%s", theme,
                              format == 0 ? "adpcm" : "pcm16");
        if (n < 0 || static_cast<unsigned>(n) >= sizeof(path)) continue;
        File* file = new File;
        if (!file) continue;
        if (file->Open(path, FA_READ | FA_OPEN_EXISTING) != FR_OK) { delete file; continue; }
        auto size = file->GetSize();
        if (size == 0 || size > kBudget || (format == 1 && (size & 1)) ||
            (format == 0 && size < 8)) { delete file; continue; }
        u32 bytes = static_cast<u32>(size);
        u32 padded = (bytes + 3) & ~3u;
        if (padded < 16) padded = 16;
        void* samples = memalign(32, padded);
        if (!samples) { delete file; continue; }
        memset(samples, 0, padded);
        bool ok = file->ReadExact(samples, bytes);
        delete file;
        if (format == 0) {
            const u8* data = static_cast<const u8*>(samples);
            ok = ok && data[2] <= 88 && data[3] == 0;
        }
        if (!ok) { free(samples); continue; }
        s->samples = samples;
        s->playToken = Token();
        s->stopToken = Token();
        s->play.count = 4;
        s->play.stop = (kMask << 8) | SND_IPC_CMD_STOP_CHANNELS;
        s->play.setup.cmd = SND_IPC_CMD_SETUP_CHANNEL;
        s->play.setup.channel = 2;
        s->play.setup.sourceAddress = samples;
        s->play.setup.timer = static_cast<u16>(-((33513982u + 22050u) / (22050u * 2u)));
        s->play.setup.loopStart = 0;
        s->play.setup.length = padded >> 2;
        s->play.setup.control = SOUNDCNT_VOLUME(80) | SOUNDCNT_PAN(64) |
            SOUNDCNT_MODE_ONCE | (format == 0 ? SOUNDCNT_FORMAT_ADPCM : SOUNDCNT_FORMAT_PCM16);
        s->play.start = (kMask << 8) | SND_IPC_CMD_START_CHANNELS;
        s->play.ack = (s->playToken << 8) | SND_IPC_CMD_ACKNOWLEDGE;
        s->stop.count = 2;
        s->stop.stop = (kMask << 8) | SND_IPC_CMD_STOP_CHANNELS;
        s->stop.ack = (s->stopToken << 8) | SND_IPC_CMD_ACKNOWLEDGE;
        DC_FlushRange(samples, padded);
        DC_FlushRange(s, sizeof(State));
        loaded = true;
    }
    if (!loaded) { free(s); return; }
    _state = s;
    __atomic_store_n(&_ready, true, __ATOMIC_RELEASE);
}

void SfxService::Tick()
{
    ++_frame;
    if (!__atomic_load_n(&_ready, __ATOMIC_ACQUIRE) || !_pending) return;
    State* s = static_cast<State*>(_state);
    if (receivedAck == s->playToken) _pending = false;
    else if (_frame - _sentFrame >= kTimeoutFrames) _disabled = true;
}

void SfxService::PlayMove()
{
    if (!__atomic_load_n(&_ready, __ATOMIC_ACQUIRE) || _disabled || _pending) return;
    if (_played && _frame - _lastPlay < 3) return;
    State* s = static_cast<State*>(_state);
    u32 irqs = rtos_disableIrqs();
    receivedAck = 0;
    bool sent = ipc_trySendFifoMessage(IPC_CHANNEL_SOUND, reinterpret_cast<u32>(&s->play));
    rtos_restoreIrqs(irqs);
    if (sent) {
        _pending = true;
        _played = true;
        _sentFrame = _lastPlay = _frame;
    }
}

void SfxService::Stop()
{
    if (!__atomic_load_n(&_ready, __ATOMIC_ACQUIRE)) return;
    __atomic_store_n(&_ready, false, __ATOMIC_RELEASE);
    State* s = static_cast<State*>(_state);
    bool sent = false, stopped = false;
    receivedAck = 0;
    for (u32 frame = 0; frame < kTimeoutFrames; ++frame) {
        if (!sent) sent = ipc_trySendFifoMessage(IPC_CHANNEL_SOUND, reinterpret_cast<u32>(&s->stop));
        if (sent && receivedAck == s->stopToken) { stopped = true; break; }
        VBlank::Wait();
    }
    if (stopped) {
        // Completion proves STOP was processed; retain an additional settling frame.
        VBlank::Wait();
        free(s->samples);
        free(s);
    }
    // On timeout retain both command storage and samples. Never free live DMA memory.
    _state = nullptr;
    _disabled = true;
    _pending = false;
}

