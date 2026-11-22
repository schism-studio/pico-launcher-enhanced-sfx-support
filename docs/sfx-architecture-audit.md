# SFX architecture audit (Step 1)

Audited revision: `6a43683a07719ee4fdc244bd9f63bc22800cc41d` on `feature/theme-sfx`; `libs/libtwl` at `aa3b2a23c10185a10449ffa685bb7933d5d5a784`.

Method: local source dumps from the maintainer's checkout plus read-only GitHub inspection. No hardware testing. No runtime code changed.

Labels: VERIFIED = seen in source. INFERRED = follows from source but not directly shown. OPEN = not yet inspected. CORRECTION = changes the blueprint.

## 1. Audio command path

- VERIFIED: `common/soundIpcCommand.h` defines START_CHANNELS, STOP_CHANNELS and SETUP_CHANNEL. A command list is a `u32` count followed by commands; setup carries source address, timer, loop start, length and control.
- VERIFIED: `arm7/source/ipcServices/SoundIpcService.cpp` runs the list in order inside `OnMessageReceived`. Start/stop take a channel mask over 16 channels; setup writes the SAD, TMR, PNT, LEN and CNT registers of the given channel.
- VERIFIED: `SoundIpcService` derives from `IpcService`, not `ThreadIpcService`, so lists run directly in the FIFO handler. It never calls `SendResponseMessage`, so there is no acknowledgement.
- VERIFIED: the only ARM9 senders on `IPC_CHANNEL_SOUND` found by the audit search are `AudioStreamPlayer.cpp` start (line 75) and stop (line 86). BGM uses channels 0 and 1 (mask 0b11). The search covered arm7, arm9, common and libs/libtwl for IPC and sound-register patterns; it does not prove channels 2-15 are unused by other code.
- VERIFIED: BGM computes the channel timer as `-((33513982 + rate) / (rate * 2))`; for 22050 Hz that is about -760.
- VERIFIED: ARM7 clears all 16 channels at start-up, and sets master volume to 0 once an exit (including the Pico Loader start) is requested.
- OPEN: the body of `ipc_sendFifoMessage` (`libs/libtwl/common/source/ipc/ipcFifoSystem.c`) was not captured, so blocking/queueing behavior is unknown. `ipc_trySendFifoMessage` also exists.
- Decision: no ARM7 change for the first proof of concept. Without an acknowledgement, a command struct may only be reused after a conservative delay (section 2), or an ARM7 acknowledgement can be added later.

## 2. Memory and teardown

- VERIFIED: the global heap is TLSF (`globalHeap.cpp`) with aligned `operator new` using `cache_align` (32). `picoLoaderBootstrap.cpp` already allocates buffers with `new (cache_align) u8[]` and calls `DC_FlushRange` before other hardware reads them; SFX buffers can follow that pattern.
- VERIFIED (suspected existing defect): `AudioStreamPlayer::FillRingBlock` calls `DC_FlushRange(&blockPtrL, ...)` and `DC_FlushRange(&blockPtrR, ...)`, passing the address of the local pointer variables instead of the sample buffers. Do not copy this pattern. BGM is audibly working, so the problem may be masked; any fix is a separate change.
- CORRECTION: the blueprint's fixed 256 KiB arena is unvalidated. Use a capped aligned allocation that falls back to silence; choose the cap after measuring free heap (`heap_dump` exists).
- INFERRED: STOP_CHANNELS is applied in the ARM7 FIFO handler. Before freeing or overwriting a sample buffer, send stop and wait at least one VBlank (`VBlank::Wait`). This is a heuristic until verified on hardware.
- OPEN: the natural end of a one-shot cannot be observed from ARM9; track clip duration in frames instead.

## 3. Lifecycle map

| Event | Existing code | Planned SFX action | Status |
|---|---|---|---|
| Construction | A boost::di configuration in a `.thumb.cpp` under `arm9/source` binds IBgmService (singleton), IAudioStreamPlayer, IAppSettingsService, IGameDataService and registers App, PicoLoaderProcess, SettingsProcess | Bind an ISfxService singleton and inject it into App | OPEN: confirm exact file |
| Startup | `App::Run` calls `LoadTheme()`, then an io task runs `BgmService::StartBgmFromConfig`, which reads `/_pico/themes/<theme>/bgm` | Load `/_pico/themes/<theme>/sfx` on the io queue using the same theme string | VERIFIED |
| Theme change | No in-App theme reload found; a comment in `BgmService` refers to a settings round trip that creates a fresh App | Reload in `App::Run`; StopAll before replacing clips | INFERRED: SettingsProcess not inspected |
| Sleep | ARM7 `checkLidSleep` calls `swiSleep`; there is no ARM9 hook and ARM7 does not touch sound registers | None initially; hardware-test sleep/wake | CORRECTION |
| App exit | `App::Run` after `MainLoop`: `WaitForQueued`, then `StopBgm` | StopAll beside StopBgm | VERIFIED |
| Process switch | `ProcessManager::Goto<T>` calls `curProcess->Exit()`; `MainLoop` runs the next process after `Run` returns | Treat App exit as the SFX teardown point | VERIFIED |

## 4. Trigger hooks

- Move: `FocusManager::Update` calls `Focus(newFocus)` when a parent's `MoveFocus` returns a view after D-pad input. `FocusManager` is a plain member of `App` with no dependencies, so a hook needs a setter or callback rather than DI. OPEN: `RomBrowserView::MoveFocus` (recycler grid) was not inspected; list selection may change without a new focused view, in which case the hook must also follow selection changes.
- Page jump (L/R): `RomBrowserController::NotifyBigStepJump` and `ConsumeBigStepJump` already flag these jumps, and `App::Update` consumes the flag every frame for the toast. VERIFIED candidate hook.
- Favorite: `RomBrowserController::ToggleFavorite`. VERIFIED single point.
- Dialogs: `App::HandleShow*Trigger` and `HandleHide*Trigger` go through `_dialogPresenter.ShowDialog` and `CloseDialog`. `DialogPresenter` has not been inspected. Some hides are issued when a sheet is replaced (`CloseSheetIfLeavingMenu`), so avoid double sounds.
- Random launch (SELECT+A) ends in `LaunchRandomGame`, which calls `LaunchFile`.

## 5. Launch path

- VERIFIED: `LaunchFile` / `LaunchSaveSlot` fire the Launch trigger, the state becomes Launching (input is no longer handled in `App::Update`), and `HandleLaunchTrigger` runs. On DS/DS Lite, or for loader-save games, an io task checks DSi-only status and the save path and can refuse the launch (toast). Only `BeginLaunch` is a committed launch.
- VERIFIED: `BeginLaunch` records game data and enqueues an io task that runs `UpdateLastUsedFilepath`, `SetPicoLoaderParams` and `LoadCheats`; the launch code ends in `gProcessManager.GotoPicoLoaderProcess`. OPEN: exact statement order inside that task.
- VERIFIED: `Goto` calls `App::Exit`, which sets `_exit` and starts a fade. `MainLoop` breaks when the fade completes; `App::Run` then waits for queued io work and calls `StopBgm`. `PicoLoaderProcess::Run` clears display registers and calls `pload_start`.
- VERIFIED: `pload_start` reads the loader binaries from the SD card, copies them to VRAM, disables interrupts and sends the `IPC_CHANNEL_LOADER` message last.
- INFERRED: a launch jingle must finish before that final message, because ARM7 mutes sound once the loader start is requested. The SD reads before it may run while audio plays; this needs hardware testing.
- Proposed insertion (not implemented): start the jingle from `BeginLaunch`, after refusal checks; build the sidecar path from the current folder and ROM name; wait or allow skip in `App::Run` between `MainLoop` and `StopBgm`.
- OPEN: fade length, the input source for skipping after input is disabled, and whether waiting in `App::Run` or `PicoLoaderProcess::Run` is better.

## 6. Blueprint corrections

1. Section 6, "call StopAll alongside every StopBgm including lid-close sleep": there is no ARM9 sleep hook. Sleep is ARM7-only. Test one-shot behavior across sleep before adding any machinery.
2. Section 6, "instantiate SfxService in App": services are DI-injected; add a binding and constructor parameter.
3. Section 5, fixed 256 KiB arena: replace with a measured, capped aligned allocation.
4. Section 7, dedicated command structs: keep them, but flush the struct memory (not a pointer variable's address) and define a reuse delay.
5. Section 8, launch handoff: use the wait location in section 5 once fade length is confirmed.
6. Section 4, channels 2-5: consistent with the observed senders; confirm on hardware. ADPCM header, loop-point and length semantics are unverified; start with PCM16.

## 7. Open items before the first patch

- `ipc_sendFifoMessage` implementation and queue behavior.
- `RomBrowserView::MoveFocus` and the recycler grid's selection handling.
- `DialogPresenter` show/close lifecycle.
- `SettingsProcess` theme switching and process re-entry.
- The DI configuration file name and whether `Makefile.arm9` compiles new subdirectories automatically.
- Baseline build and DS Lite boot (not yet done).

## 8. Proposed first patch (Move only)

- New: `arm9/source/sfx/` contract, policy, service implementation.
- Edit: DI configuration, `App.h`, `App.cpp`, and `FocusManager` (callback or setter).
- No ARM7 changes.
- Defer: dialogs, launch jingles, ADPCM encoder, sleep handling beyond testing.
