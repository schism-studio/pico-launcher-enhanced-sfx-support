# SFX implementation and integration

This guide supersedes the original placeholder-only plan. For installation, filenames, asset formats, discovery rules, selection, and fallback order, see [Sound banks and SFX Options](SoundBanks.md).

## Version and validation boundary

The customized Default-bank resolver and theme-style selector were inspected in the local export `pico-sfx-options-final-source-20261122-212833-d0308f.txt`. A separate final selector patch adds the SFX off card, removes the lower-left toggle, and adds the SFX Options heading. Those final UI changes are documented as the target behavior; applying that patch, building it with the target toolchain, and testing it on DS hardware must be verified separately. A documentation commit does not upload the local implementation or certify the branch build.

The final UI patch reported 24 mocked selection/save/return/recycling checks and 10 sanitizer-backed artwork checks. These are host checks, not ARM9/ARM7 hardware validation.

## Responsibilities

- `arm9/source/sfx/SfxClass.h`: fixed semantic roles, standard filename stems, fallback roles, volume, channel policy, and cooldowns.
- `SfxAssets.h/.cpp`: source roots, safe folder names, supported file inspection, and per-role layer loading. The stock layer first tries `/sfx/Default/`, then embedded stock.
- `SfxPackRepository.h/.cpp`: bounded discovery and sorting of immediate bank directories under `/sfx/`. The directory defines the option; valid audio files do not define whether a card is listed.
- `StockSfx.cpp`: compiled stock clips, independent of SD assets.
- `SfxService.h/.cpp`: UI clip loading, semantic fallback resolution, prioritized requests, bounded channel playback, launch sequencing, stop and acknowledgement ownership.
- `LidSfxService.h/.cpp`, ARM7 lid support, and `common/lidSfxConfig.h`: separate sleep/wake cue lifecycle.
- `settings/SfxMenuState.h/.cpp`: saved versus proposed settings and checked-save state transitions.
- `settings/SfxSettingsProcess.h/.cpp`: scan/artwork/save task ownership, safe return flow, screen resources, and preview publication.
- `settings/views/SfxSettingsView.h/.cpp`: theme-style bank cards and selection actions.
- `settings/SfxBankArtwork.h/.cpp`: built-in package/speaker art, optional BMP loading, and the final patch's built-in muted-speaker art.
- Existing application, browser, dialog, settings, loader/process, and ARM7 sound integrations dispatch events and manage transitions.

## Playback and ownership rules

UI hooks request semantic events rather than opening files or allocating sample buffers. Loading occurs outside those hooks. Among ordinary UI requests, more specific events beat incidental navigation within a frame; launch follows a dedicated exclusive path.

The inspected UI service uses channels 2-5 and a 256 KiB SD-sample arena. Lid cues have a separate 48 KiB sample budget. These are implementation budgets, not an allowance to load every maximum-size file. Layers are loaded in priority order; capacity/allocation/read failures can cause a lower layer or silence to be used.

Commands and samples published for ARM7 remain immutable while hardware may use them. Stop acknowledgement precedes freeing sample ownership. The existing timeout/quarantine behavior deliberately retains uncertain buffers rather than freeing memory still potentially read by hardware. Join IO work before destruction or transition. Do not bypass these rules to simplify a menu change.

The final selector patch does not change playback, the Default resolver, settings serialization, or the existing renderer fix. It reuses the checked-save/application flow. Failed saves do not publish the proposed settings as successful.

## Scope exclusions

The old plan included per-game `.launch.*` ROM sidecars. This documentation does not advertise those as implemented. The supported contract here is semantic launcher SFX, selected banks, theme overrides, customizable Default assets, embedded fallback, and sleep/wake cues. Background music is separate from this SFX option.

Do not assume an older converter scaffold produces valid DS ADPCM. Raw PCM16 can be prepared with the FFmpeg command in the bank guide. Use a verified DS-compatible encoder for ADPCM.

## Build and release checklist

1. Apply the intended source patches and review the source diff. Preserve the working renderer and audio-ownership fixes.
2. Build in the configured UCRT64/devkit environment with `make -j4`; retain the complete log and confirm a zero exit status.
3. Verify boot, ordinary navigation, rapid input, action priorities, and BGM coexistence on hardware.
4. Verify Default, a partial named bank, missing folders, invalid files, preferred-format failure, and memory-budget exhaustion behavior.
5. Verify SFX off, retained bank preference, re-enabling through Default/named selection, and persistence after restart.
6. Verify Back during scan, artwork IO, and save; save failures/recovery; and safe process return.
7. Verify sleep/wake, theme changes, launch playback, and clean game handoff without stale sample access.
8. Verify the final title, crossed-out icon, touch/D-pad navigation, row recycling, and malformed artwork on the DS screens.
9. Commit only distributable assets with provenance/license information. Do not publish third-party test sounds without permission.

Host validation does not replace any of these hardware checks.
