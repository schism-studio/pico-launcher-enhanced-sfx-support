# SFX research and updated planning/protocol

Status: implementation decisions, not implemented or hardware-validated.
Source basis: original UI SFX and game launch blueprint; local audit dumps at commits 6a43683 and 1e5724e; libtwl aa3b2a23c10185a10449ffa685bb7933d5d5a784.
This document supersedes conflicting proposals in sfx-architecture-audit.md.

## Philosophy and preserved requirements

Prioritize stability, bounded resources and the existing launcher architecture over general-purpose modularity.
Keep theme-bundled UI SFX, ROM-adjacent per-game launch overrides, no sound-pack selector and no settings schema change.
Support mono 22050 Hz raw signed little-endian PCM16 and DS-compatible IMA-ADPCM through one playback path. Prefer valid ADPCM, then PCM16. ADPCM priority is a storage/memory decision, not a proven CPU-speed advantage.
No runtime conversion or software mixer.
Use fixed semantic classes and a compile-time policy table. No event bus, codec plugin architecture or configurable routing.
Use one concrete SfxService owned by App; remove the unused ISfxService placeholder in the functional patch. Pass direct references/pointers where needed.

## Verified source findings

- AudioStreamPlayer.h: BGM configures channels 0 and 1.
- common/soundIpcCommand.h and SoundIpcService.cpp: counted command lists support setup, start and stop; ARM7 executes them sequentially. This alone is not an atomicity guarantee against all interrupts or other hardware activity.
- libtwl ipcFifoSystem.c: ipc_sendFifoMessage retries ipc_trySendFifoMessage until FIFO space is available. Enqueue success does not acknowledge execution.
- SoundIpcService has no explicit completion response.
- arm7/source/main.cpp: checkLidSleep owns lid sleep; no explicit SFX stop or ARM9 notification exists there.
- SettingsController::SelectTheme saves the selection and requests Goto<App>; browser SFX reload belongs on App entry.
- DialogPresenter::Update owns actual visible/closing transitions; future sheet sounds belong at transitions, not repeated close requests.
- Makefile.arm9 recursively finds C++ files under arm9/source and common.
- globalHeap supports 32-byte aligned allocation. This does not establish free memory headroom.
- libtwl soundChannel.h documents PCM16/ADPCM and word-counted lengths, with a minimum length of 16 bytes. Validate format-specific length/header rules before implementation.
- AudioStreamPlayer::FillRingBlock passes &blockPtrL and &blockPtrR to DC_FlushRange. Track as a suspected existing defect; do not copy it or silently bundle an unrelated BGM fix.

## Channel policy and panning

Preserve the blueprint allocation: Move/PageJump use channel 2; action effects use the small pool on channels 3-5; Launch is exclusive. Verify remaining channel users before integration.
The first patch uses channel 2 only because it implements Move only, not because the final design abandons action overlap.
Use one centered mono channel per voice. Do not duplicate channels merely for loudness. The libtwl pan documentation describes per-speaker attenuation; it does not establish a fixed loudness difference relative to arbitrary BGM. Tune asset levels and policy volumes on hardware.

## Memory, loading and failures

Use one aligned contiguous theme arena allocated once per browser session and sized to validated assets. Retain 256 KiB as a proposed ceiling, not an unconditional reservation or a validated budget. Confirm headroom before settling the limit.
Load core assets before optional assets. Reject malformed, empty, oversized or unreadable clips safely. Missing assets remain silent or use the fixed semantic fallback.
No allocations, SD reads or logging per ordinary playback event. Load through the existing I/O queue; publish a fully prepared immutable asset set before playback can access it.
Use frame cooldowns and an early return when SFX are unavailable.
Keep the theme launch fallback intact while attempting a ROM-sidecar override. Prefer a separate capped temporary launch buffer; its budget remains to be measured. Reject oversized overrides rather than exhausting memory.

## IPC lifetime and stop protocol

Use ordered STOP -> SETUP -> START for one-shots; validate the precise hardware register behavior during implementation.
Keep sample data and submitted command storage in ARM7-accessible memory with required alignment and cache flushing. Never pass stack command lists or reuse mutable storage before consumption is established. Prebuild immutable per-clip commands where practical.
Add a narrowly scoped completion mechanism if needed: a small ARM7 acknowledgement extension is allowed. Preserve compatibility with existing BGM command lists and serialize/identify SFX requests explicitly. The exact wire format, reply handling, timeout and memory barriers must be reviewed before coding.
A single VBlank delay is not proof of command completion. Stop acknowledgement must establish the required hardware quiescence before buffers are freed or overwritten; verify any additional hardware settling requirement. A timeout must not free possibly-live buffers. Fail closed: disable further SFX and retain memory rather than risking invalid hardware reads.
Do not claim that hardware testing alone proves race freedom.

## Lifecycle and launch

Load on App entry. Stop and establish completion before App-owned resources are released or replaced. Keep lid handling in its existing ARM7 owner and add only necessary audio coordination. No power-management rewrite.
For launch: accept/refuse the game first; prepare the chosen override through the I/O path; stop BGM and UI SFX; play override or theme fallback; allow skip with a finite duration cap; confirm stop before loader handoff.
Do not overlap loader-binary reads with the jingle as an initial optimization. Do not block the I/O queue waiting for work queued onto itself.
Keep a main-loop input/update path for bounded playback and skip.
Final source locations and request/completion sequencing remain implementation-review items.

## Build and test sequence

1. Build the current scaffold baseline using the existing Build Pico Launcher GitHub Actions workflow; it runs make in skylyrac/blocksds:slim-v1.16.0 and uploads LAUNCHER.nds and _pico. Actions enablement and run success must be checked, not assumed.
2. Back up the installed launcher and test baseline boot, BGM, navigation, theme round trip, sleep/wake and game launch on DS Lite.
3. Inspect concrete recycler selection handling before choosing the Move hook. Do not use a raw focus-pointer comparison as the only success criterion without verification.
4. Implement the concrete service, bounded dual-format loader, command lifetime protocol, one Move hook and startup/teardown integration. Exercise PCM16 first, then verified DS ADPCM.
5. Build, review warnings/errors and test absent assets, rapid input over BGM, sleep/wake, theme switching and game launch.
6. Add Confirm/Back and remaining semantic hooks only after Move is stable. Then implement per-game launch playback.

The current documents and placeholders do not provide working SFX. Baseline builds can proceed while the remaining implementation review is completed.
No further broad source dumps are required; inspect specific files for remaining questions.
