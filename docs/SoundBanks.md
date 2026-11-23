# Sound banks, filesystem standards, and SFX Options

This guide describes the customized Default-bank resolver and the final SFX Options selector design. The resolver is present in the inspected local source export; the Off-card/title UI is supplied by the subsequent final selector patch. Use a build containing those changes. Documentation publication alone does not confirm that local source was pushed, that the final patch was applied, or that hardware testing passed.

## 1. SD filesystem layout

Paths below are relative to the SD card root, not to the launcher executable and not automatically to the Git repository root.

```text
/sfx/
  Default/                  # customizable stock layer; one built-in Default option
    move.pcm16
    confirm.pcm16
    back.pcm16
    ...                     # any other standard roles, independently optional
    icon.bmp                # optional package icon
    preview.bmp             # optional package preview
  MyBank/                   # immediate folder name becomes the named-bank option
    move.adpcm
    confirm.pcm16
    ...
    icon.bmp
    preview.bmp
/_pico/themes/MyTheme/sfx/
  move.pcm16                # theme-specific overrides
  ...
```

There is no manifest required for a bank. Standard directory locations, folder names, filename stems, and supported extensions form the interface. The selected bank is remembered by folder name. Renaming a folder changes that identity; reselect it afterward.

If the fork stores distributable stock assets at repository path `sfx/Default/`, copy that directory to SD path `/sfx/Default/`. Committing a directory does not deploy it to the card. This documentation does not claim the stock assets have already been committed.

## 2. How options are discovered

`SfxPackRepository` scans the immediate children of `/sfx/`:

- Each safe directory defines one named bank. Nested folders are not recursively indexed as extra options.
- Default is reserved case-insensitively and omitted from the named-bank scan, because the menu already provides a single Default card.
- Ordinary files at `/sfx/` do not become options.
- Empty, partial, and invalid-audio directories are still listed. Discovery is directory-based; audio validity is checked separately during loading.
- Named banks are sorted case-insensitively, with a maximum of 64 named banks. Scanning stops after 4096 root entries and can report truncation.
- Folder names must be 1-255 bytes, not `.` or `..`, contain no control characters or `\ / : * ? " < > |`, and must not end in a space or period. Short, portable names are recommended.

The final menu places the virtual SFX off card first, then Default, then named banks. Off is a UI action, not a reserved asset folder or a saved pack name. Do not create a folder expecting it to act as the global mute control.

Creating a named folder makes the option discoverable on the next menu scan. Adding a standard role file changes that bank's contribution when sounds are next loaded. Arbitrary audio filenames do not add new semantic roles or menu actions; those roles are defined in source.

## 3. Selecting an option

| Option | Enabled flag | Saved bank |
| --- | --- | --- |
| SFX off | false | retain the previously saved bank |
| Default | true | `default` |
| Named bank | true | that directory's name |

Highlighting a card previews it; it does not save or toggle playback. Activating a card uses the checked-save and return/application flow. Selecting an already-saved option need not rewrite settings. A failed save is not treated as a successful application. Reopening while disabled selects the Off card. Selecting Default or a named bank explicitly re-enables SFX.

The lower-left toggle is removed by the final UI patch. The upper Back control remains. Default's description becomes `Theme, then stock`, and the top heading is `SFX Options`. The Off card and its preview use a built-in crossed-out speaker. No SD image is needed for Off.

This is the SFX control, not a master background-music switch.

## 4. Per-role resolution, not whole-bank replacement

With Default selected:

```text
active theme -> /sfx/Default -> embedded stock -> semantic fallback -> silence
```

With a named bank selected:

```text
selected bank -> active theme -> /sfx/Default -> embedded stock -> semantic fallback -> silence
```

With SFX off selected: no SFX playback, including lid cues.

Each requested semantic role is resolved independently. A partial bank is supported. For example, a bank containing only `move.pcm16` overrides navigation while missing Confirm, Back, and other roles can still resolve from the theme or stock layers.

Within each filesystem layer the loader tries valid `.adpcm` first, then `.pcm16`. An absent, rejected, unreadable, or unallocatable candidate does not require the whole bank to fail. Loading is subject to finite sample-memory budgets.

Semantic fallback is later than the requested role's source layers. A missing `pagejump` searches its own bank/theme/stock candidates before falling back to the already-resolved Move role. It does not immediately use the selected bank's Move clip while ignoring an available lower-layer PageJump clip.

Default is customizable stock, not a force-stock mode: an active theme's matching role still wins. To override a theme deliberately, put the desired role in a named bank and select it.

## 5. Standard role filenames

Use each stem with `.adpcm` or `.pcm16`:

| Stem | Semantic role | Fallback role if unresolved |
| --- | --- | --- |
| `move` | navigation | none |
| `confirm` | confirmation | none |
| `back` | back/cancel | none |
| `launch` | game launch | confirm |
| `pagejump` | page navigation | move |
| `toggleon` | enable action | confirm |
| `toggleoff` | disable action | back |
| `sheetopen` | open sheet/dialog | confirm |
| `sheetclose` | close sheet/dialog | back |
| `favorite` | favorite action | confirm |
| `warning` | warning | confirm |
| `boundary` | navigation boundary | none |
| `sleep` | lid sleep cue | separate lid path |
| `wake` | lid wake cue | separate lid path |

The event's semantic classification is defined by the calling UI code; renaming a sound does not change which event triggers it. A fallback supplies an asset while the originating UI role retains its playback policy. Sleep/wake use the common source hierarchy but a separate lifecycle and budget, rather than the ordinary UI fallback table.

## 6. Audio file contract

Use mono 22050 Hz audio:

- `.pcm16`: raw signed 16-bit little-endian PCM, without a WAV/container header. Byte count must be even.
- `.adpcm`: DS-compatible raw ADPCM data with the expected predictor/index header. The inspected validator requires at least 8 bytes, an initial step index no greater than 88, and a zero reserved header byte. A generic WAV/IMA container is not the same format.
- Files must be nonempty and no larger than 256 KiB. This file ceiling does not override duration or total arena limits.
- Ordinary UI/launch clips must fit 120 timing frames, approximately 2 seconds. Sleep/wake must fit 24 timing frames, approximately 0.4 seconds. The validator computes duration from hardware-padded sample size; leave margin near the limit.

Prepare PCM16 with FFmpeg:

```sh
ffmpeg -i input.wav -ac 1 -ar 22050 -c:a pcm_s16le -f s16le move.pcm16
```

Trim the source to the intended cue length before conversion. Do not just rename WAV, MP3, OGG, or arbitrary ADPCM files to a supported extension. An older converter scaffold is not evidence of a working DS ADPCM encoder.

The normal UI service has a 256 KiB SD-sample arena; lid cues have a separate 48 KiB budget. These are total budgets. Embedded clips do not require copying their sample data into that SD arena. Higher-priority layers receive allocation opportunities before lower layers. Retain small assets suitable for DS hardware.

## 7. Optional bank artwork

Place images directly in `/sfx/<bank>/`; Default artwork comes from `/sfx/Default/`, not the active theme.

- `icon.bmp`: 32x32, 4-bit indexed BMP, at most 16 KiB, in the uncompressed form accepted by the BMP parser. Palette index 0 is treated as transparent when painting the package icon.
- `preview.bmp`: 128x128 uncompressed 24-bit or 32-bit BMP, at most 256 KiB. Top-down and bottom-up files are supported. The decoder uses RGB; alpha is not used for blending. The image is fitted to the package's 96x96 inner area.
- Unsupported, missing, corrupt, or unreadable artwork falls back to built-in package/speaker art. A missing preview can still leave a successfully loaded custom package icon visible.

In the inspected selector, list-row icons are built-in speaker graphics; the optional bank BMPs customize the top package preview. The final Off row uses the built-in crossed-out speaker and does not read artwork files.

## 8. Make a bank from Default

1. Keep a distributable Default directory containing any desired stock roles and optional artwork.
2. Copy it to `/sfx/MyBank/` on the SD card.
3. Replace the role files you want to customize. Remove unnecessary files to make a partial bank; lower layers fill gaps.
4. If replacing a role with PCM16, remove or replace the same stem's ADPCM file too, otherwise a valid old ADPCM candidate wins first.
5. Reopen the sound menu and select MyBank. Re-enter/reload after editing assets; this is not continuous hot reload.
6. Check navigation, confirmation, back, launch, and lid behavior. Test that missing-role fallback is intentional.

Do not assume silence can be forced by deleting a role: lower layers may supply it. Use SFX off for global SFX silence.

## 9. Troubleshooting and release discipline

- Missing bank card: verify an immediate `/sfx/<name>/` directory, a safe non-Default name, scan limits, and SD read status.
- Card appears but a replacement is not heard: verify filename, preferred ADPCM candidate, raw format, duration, sample budget, active selection, and reload. Card presence does not prove audio validity.
- Default appears to ignore an edit: an active theme may have a higher-priority matching role.
- Wrong picture: check BMP dimensions, bit depth, compression, and the bank folder; preview art does not change audio selection.
- Do not bypass checked saves, IO joins, acknowledgement ownership, or uncertain-buffer quarantine to work around a failure.

Keep source audio/provenance and licensing information. Commit only assets you have permission to distribute. Keep generated binaries, build logs, source dumps, patch helpers, and backups out of ordinary source commits unless intentionally released.

See [implementation and validation notes](sfx-implementation.md) for the component map and hardware checklist. These instructions do not certify that the final selector patch has been applied, built, or tested on the DS.
