# Theme SFX and per-game launch audio

Status: placeholders only. No audio feature is implemented or tested.

## Scope

- Bundle UI sounds inside the active theme; no separate pack selector or settings schema change.
- Store per-game launch overrides beside ROMs, outside themes.
- Plan for mono 22050 Hz PCM16 and DS-compatible IMA-ADPCM, with ADPCM preferred after validation.
- Keep playback within the launcher; stop before game handoff.

## Planned assets

```text
/_pico/themes/<theme>/sfx/
  move.adpcm       # or move.pcm16
  confirm.adpcm    # or confirm.pcm16
  back.adpcm       # or back.pcm16
  launch.adpcm     # or launch.pcm16
  # Optional: pagejump, toggleon, toggleoff, sheetopen, sheetclose,
  # favorite, warning, boundary, each using the same extensions.
/roms/Game.nds
/roms/Game.launch.adpcm   # optional override
/roms/Game.launch.pcm16   # fallback format
```

## Source audit and integration

- [ ] Verify build configuration and establish an untouched hardware-tested build.
- [ ] Inspect BgmService, AudioStreamPlayer, sound IPC definitions, and ARM7 command processing.
- [ ] Validate proposed ARM9-only implementation, channels 2-5, and 256 KiB arena budget.
- [ ] Define IPC acknowledgement, command reuse, cache maintenance, and sample lifetime rules.
- [ ] Wire startup/theme reload in App, with safe stop on sleep and theme changes.
- [ ] Select one navigation hook in FocusManager or romBrowser; avoid duplicate sounds.
- [ ] Add Confirm, Back, Launch, PageJump, Favorite, toggles, and warning hooks.
- [ ] Add dialog/sheet lifecycle hooks in DialogPresenter where appropriate.
- [ ] Inspect App and PicoLoaderProcess for bounded/skippable launch playback and safe handoff.

## Loading and fallback

- [ ] Missing SFX folders remain silent without breaking existing themes.
- [ ] Define invalid, empty, truncated, and oversized asset handling.
- [ ] Try valid ADPCM first, then PCM16; define semantic fallback policies.
- [ ] Preload theme clips with validated alignment, padding, and cache flushing.
- [ ] Load only the selected game's launch override on demand.
- [ ] Preserve the theme launch fallback if override loading fails.
- [ ] Do not free or overwrite samples while hardware can still read them.

## Conversion

Prepare a first PCM16 asset with FFmpeg:

```sh
ffmpeg -i input.wav -ac 1 -ar 22050 -f s16le -c:a pcm_s16le move.pcm16
```

The Python converter is an explicit non-working stub. Validate a DS-compatible encoder before enabling ADPCM assets. Keep source audio and licensing information separate from runtime files; do not commit third-party copyrighted test clips.

## Execution checkpoints

1. Baseline: build unmodified sources and verify boot on DS hardware.
2. Move proof of concept: service, theme loading, lifecycle handling, and one hook.
3. UI coverage: remaining semantic sound hooks and volume/cooldown tuning.
4. Launch overrides: ROM sidecars, theme fallback, skip/duration behavior, clean handoff.
5. Validate both audio formats and finish asset/deployment documentation.

## Hardware test checklist

- [ ] No SFX folder: normal launcher behavior.
- [ ] Move sound over BGM without interference.
- [ ] Rapid navigation without static, crashes, or duplicate triggers.
- [ ] Sleep/wake without stuck audio or invalid sample access.
- [ ] Theme changes with and without SFX.
- [ ] Missing, invalid, and oversized clips handled safely.
- [ ] Game-specific launch clip and theme fallback both work.
- [ ] Launch audio can finish or be skipped within a defined maximum duration.
- [ ] All launcher audio stops before ROM handoff.
- [ ] Repeat tests with PCM16 and verified DS ADPCM.
