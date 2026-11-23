#include "common.h"
#include <libtwl/sound/soundChannel.h>
#include "soundIpcCommand.h"
#include "SoundIpcService.h"
#include "lidSfx7.h"
#include <libtwl/ipc/ipcFifoSystem.h>

void SoundIpcService::OnMessageReceived(u32 data)
{
    const u32* commandList = reinterpret_cast<const u32*>(data);
    u32 cmdCount = *commandList++;
    for (u32 i = 0; i < cmdCount; i++)
    {
        u32 cmdValue = *commandList++;
        u32 cmd = cmdValue & 0xFF;
        u32 cmdArg = cmdValue >> 8;
        switch (cmd)
        {
            case SND_IPC_CMD_START_CHANNELS:
            {
                u32 channelsMask = cmdArg;
                for (u32 j = 0; j < 16; j++)
                {
                    if (channelsMask & (1u << j)) {
                        if (j >= 2 && j <= 5 && !lidSfxAllowUi()) continue;
                        snd_startChannel(j);
                    }
                }
                break;
            }
            case SND_IPC_CMD_STOP_CHANNELS:
            {
                u32 channelsMask = cmdArg;
                for (u32 j = 0; j < 16; j++)
                {
                    if (channelsMask & (1u << j))
                        snd_stopChannel(j);
                }
                break;
            }
            case SND_IPC_CMD_LID_SFX_CONFIG:
            {
                const auto* config = reinterpret_cast<const LidSfxConfig*>(*commandList++);
                if (cmdArg != 0 && i + 1 == cmdCount) {
                    lidSfxConfigure(config);
                    ipc_trySendFifoMessage(IPC_CHANNEL_SOUND, kLidSfxAckFlag | cmdArg);
                }
                break;
            }
            case SND_IPC_CMD_ACKNOWLEDGE:
            {
                // Keep the FIFO IRQ bounded: never spin waiting for reply space.
                // If the reply cannot be queued, ARM9 must time out safely
                // without releasing possibly-live command/sample storage.
                if (cmdArg != 0 && i + 1 == cmdCount)
                    ipc_trySendFifoMessage(IPC_CHANNEL_SOUND, cmdArg);
                break;
            }
            case SND_IPC_CMD_SETUP_CHANNEL:
            {
                u32 channel = cmdArg;
                REG_SOUNDxSAD(channel) = *commandList++;
                REG_SOUNDxTMR(channel) = *commandList++;
                REG_SOUNDxPNT(channel) = *commandList++;
                REG_SOUNDxLEN(channel) = *commandList++;
                REG_SOUNDxCNT(channel) = *commandList++;
                break;
            }
        }
    }
}
