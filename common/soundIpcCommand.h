#pragma once

enum SoundIpcCommand
{
    SND_IPC_CMD_START_CHANNELS,
    SND_IPC_CMD_STOP_CHANNELS,
    SND_IPC_CMD_SETUP_CHANNEL,
    // Must be the final command in an SFX list.
    // Upper 24 bits carry a nonzero request ID.
    // Reply indicates command processing, not clip playback completion.
    SND_IPC_CMD_ACKNOWLEDGE
};

struct snd_ipc_cmd_setup_channel_t
{
    u32 cmd : 8;
    u32 channel : 24;
    const void* sourceAddress;
    u32 timer;
    u32 loopStart;
    u32 length;
    u32 control;
};
