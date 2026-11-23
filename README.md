# Pico Launcher Enhanced - SFX Support

This is schism-studio's SFX-focused fork of [Pico Launcher Enhanced](https://github.com/rasalopa/pico-launcher-enhanced), built on Pico Launcher by the LNH team. The `feature/theme-sfx` branch adds semantic launcher sound effects, selectable sound banks, theme overrides, customizable stock sounds, and sleep/wake cues while retaining the Enhanced launcher features documented below.

## SFX support in this branch

- Theme-style sound-bank selector with a separate `SFX off` option and crossed-out speaker icon.
- `Default` uses the active theme first, then the editable stock bank and embedded fallback. Its description is `Theme, then stock`.
- Named banks override matching theme sounds without requiring a complete replacement sound set.
- Selecting Off disables SFX while retaining the saved bank. Selecting Default or a named bank enables SFX and applies that choice through the checked-save/return flow.
- Highlighting a card previews its artwork; it does not save the choice. The top screen is headed `SFX Options`.
- Standard role filenames cover navigation, confirmation, back, launch, page changes, toggles, sheets, favorites, warnings, boundaries, and sleep/wake cues.
- Optional bank BMPs customize the top package preview. Built-in artwork handles missing or invalid images.
- Background music remains separate from the SFX setting.

Source availability is not release certification. This README does not claim that a tagged downloadable binary includes these changes or that the latest merged source has passed a full target build and DS hardware test. Use a build from this branch and verify it on your console. The inherited release badges, screenshots, and release-installation instructions below are not evidence of an SFX-enabled release.

### SD layout and bank discovery

Paths are relative to the SD card root:

```text
/sfx/
  Default/                       # editable stock layer and bank template
    move.pcm16
    confirm.pcm16
    back.pcm16
    icon.bmp                     # optional
    preview.bmp                  # optional
  MyBank/                        # immediate folder name becomes a menu option
    move.adpcm
    confirm.pcm16
/_pico/themes/MyTheme/sfx/        # theme-specific sounds
  move.pcm16
```

No bank manifest is required. Safe immediate directories under `/sfx/` define named-bank options; a directory can be empty or partial and still appear. Validity of its audio is checked separately when loading. Default appears once as a built-in option, and Off is a virtual control, not an asset directory. Arbitrary audio filenames do not create new event types.

The editable `/sfx/Default/` folder is intended to be customizable and reusable as a template. To make a bank, copy it to `/sfx/MyBank/`, replace the standard role files you want, reopen the sound menu, and select MyBank. If a repository copy is maintained at `sfx/Default/`, it must still be copied to the SD card; this README does not assert that the assets are included or deployed.

### Which sound wins?

Resolution is independent for every sound role:

```text
Default selected:
  active theme -> /sfx/Default -> embedded stock -> semantic fallback -> silence

Named bank selected:
  selected bank -> active theme -> /sfx/Default -> embedded stock -> semantic fallback -> silence

SFX off selected:
  no SFX playback
```

Within each filesystem layer, valid `.adpcm` is preferred over `.pcm16`. Missing or rejected clips can fall through to lower layers. A partial bank therefore overrides only the sounds it supplies. Default does not force stock sounds over a theme; use a named bank for that priority.

### Asset standards and setup

Use mono 22050 Hz raw signed little-endian PCM16 (`.pcm16`, not a WAV container), or verified DS-compatible raw ADPCM (`.adpcm`). Ordinary clips must fit approximately 2 seconds; sleep/wake clips approximately 0.4 seconds. File validation, padding, and total memory budgets also apply.

The standard stems are `move`, `confirm`, `back`, `launch`, `pagejump`, `toggleon`, `toggleoff`, `sheetopen`, `sheetclose`, `favorite`, `warning`, `boundary`, `sleep`, and `wake`.

Optional `icon.bmp` is 32x32, 4-bit indexed; optional `preview.bmp` is 128x128, uncompressed 24-bit or 32-bit. See the full guide for limits and transparency behavior. The BMPs customize the top preview; list-row speaker icons are built in.

For an SFX build, use the source-building instructions below, then deploy `LAUNCHER.nds` as `_picoboot.nds` where appropriate and copy any desired bank assets to SD `/sfx/`. Commit and distribute only sounds/artwork you have permission to share. Merely pushing source does not deploy SD files or create a binary release.

- [Sound banks and filesystem reference](docs/SoundBanks.md): discovery rules, selection, all filenames, fallback order, PCM conversion, BMP standards, and troubleshooting.
- [SFX implementation and validation](docs/sfx-implementation.md): component map, task/audio ownership, budgets, and build/hardware checklist.

Per-game `.launch.*` ROM sidecars from the earlier planning documents are not advertised as an implemented feature of this system.

---

## Pico Launcher Enhanced foundation

The original Enhanced overview, features, instructions, and contributor credits are preserved below.

[![Latest release](https://img.shields.io/github/v/release/rasalopa/pico-launcher-enhanced?display_name=tag&sort=semver&label=release)](../../releases/latest)
[![Downloads](https://img.shields.io/github/downloads/rasalopa/pico-launcher-enhanced/total?label=downloads)](../../releases)
[![License](https://img.shields.io/github/license/rasalopa/pico-launcher-enhanced)](LICENSE.txt)

A feature fork of [Pico Launcher](https://github.com/LNH-team/pico-launcher) by the LNH team, adding library features inspired by modern consoles, like favorites, play stats and recently played, while staying fully compatible with stock SD cards. It is a drop-in `_picoboot.nds` replacement, and all upstream features remain intact.

![The statistics panel: the folder's game count, totals, the most launched games and the last one played](docs/images/enhanced/Statistics.png)
![The menu, with Mario Kart DS and its star, heart and check on the top screen: recently played, favorites, statistics, delete game, a new save for that game and the two filters](docs/images/enhanced/Menu.png)
![The about sheet: Pico Launcher by the LNH team, Enhanced by rasalopa, where the build came from, and a cheat sheet of the controls that have no button](docs/images/enhanced/About.png)

*Taken on the console with the launcher's own screenshot key (hold START).*

## Features

Everything upstream Pico Launcher offers (display modes, [custom icons, banners & covers](docs/Customization.md) for games *and folders*, [themes](docs/Themes.md), [cheats](docs/Cheats.md), [file associations](docs/FileAssociations.md); see [Usage](docs/Usage.md)), plus:

- **Go back to the launcher from inside a game** (optional): with Pico Loader Enhanced, the second zip in the release, hold L + R + Right + Select for a moment in a DS game; see [Installation](#optional-the-in-game-exit)
- **A menu instead of a row of icons**: the app bar keeps back and display settings and gains a three-dot button; recently played, favorites, statistics, deletion, a new save for the highlighted DS game and the two filters live in the sheet it opens, each with its name
- **About sheet**: from the menu's title row: who made what, the exact build and the repository it came from, and a cheat sheet of the controls that have no button
- **Jump by initial**: press L or R to jump to the next initial letter in a folder sorted by name; the letter you land on shows for a moment at the bottom of the touch screen
- **Game count** of the current folder, at the top of the statistics panel
- **Random game launch** with SELECT + A
- **Favorites**: press X on a game; a heart shows on the top screen
- **Completed games**: hold X on a game; a green check shows on the top screen
- **Favorites and completed filters**: Only favorites and Only completed in the menu, red or green while on
- **Favorites panel**: every favorite across all folders, from the menu; tapping one jumps to it
- **Recently played panel**: from the menu; tapping an entry jumps to the game
- **Statistics panel**: totals, most-played games and the launcher version, from the menu
- **Screenshots**: hold START for about half a second to save both screens to `/_pico/screenshots`
- **Per-game launch tracking**: launch count and last-played date, kept per game; the recently played panel shows each game's date, and the statistics panel the counts of the three most launched games; can be switched off in `settings.json`
- **Play time** counted while you play, with Pico Loader Enhanced and its in-game exit: per game in the favorites and statistics panels, and the total in statistics
- **Game deletion**: from the menu, with confirmation; removes the ROM and, for a DS or GBA game, its save; a DS game's extra saves go too
- **ROMs run by an emulator count as games**: GB, SNES or any ROM opened through a file association can be marked, deleted and picked at random; see [FileAssociations.md](docs/FileAssociations.md)
- **Theme deletion**: from the theme selector, with confirmation; the theme in use and the two that come with the launcher are kept
- **A saves folder**: set `"saveLocation": "saves"` in `settings.json` and DS saves live in a `saves` folder next to the games, the layout TWiLight Menu++ uses, so both launchers can share one card; see [Usage.md](docs/Usage.md#settings)
- **Save slots**: up to nine extra saves per DS game, `.sav1` to `.sav9` as TWiLight Menu++ names them; A on a game that has any lets you pick the one to play, and New save in the menu starts the game on a new one; see [Save slots](docs/Enhanced.md#save-slots)
- **Brightness control**: set the DS Lite's backlight level from display settings
- **Hide empty folders**: optional; folders with nothing playable inside are left out of the listing
- **Cheats list that reads better**: the list wraps around at both ends, and X turns every cheat off at once
- **Game markers on the top screen**: a gold star for the most launched game, a heart for a favorite and a check for a completed one, above the game's icon; a [custom theme](docs/Themes.md) can move or hide them
- **Per-folder background music**: drop a `bgm.bcstm` inside a folder
- **Time-of-day theme backgrounds**: optional night variants shown from 20:00 to 6:59
- **Per-file game data**: favorites, completed marks and play stats belong to the ROM file, so two copies of a game never share them

See [Enhanced features](docs/Enhanced.md) for details on each feature.

## Installation

1. Download `Pico_Launcher_Enhanced.zip` from the [latest release](../../releases/latest).
2. Take `LAUNCHER.nds` out of it, rename it to `_picoboot.nds` and place it in the root of your SD card, replacing the existing one.

No other changes to your SD card are needed: themes, covers and the `_pico` folder from a stock setup keep working as-is.

> [!NOTE]
> To use Pico Launcher, the Pico Loader files (`aplist.bin`, `patchlist.bin`, `savelist.bin`, `picoLoader7.bin` and `picoLoader9.bin`) must also be present in the `/_pico` folder on your SD card.

On another flashcart, like an R4 or a DSTT, the steps are the same when the card starts `_picoboot.nds` from the root, as the DSTT setup from flashcarts.net does. Keep the Pico Loader files made for that card in `/_pico`: LNH has a zip for each card in the [Pico Loader releases](https://github.com/LNH-team/pico-loader/releases).

### Optional: the in-game exit

The release also has a second zip, Pico Loader Enhanced: a version of Pico Loader that lets you go back to the launcher from inside a DS game by holding L + R + Right + Select for a moment. To use it, copy its `picoLoader7.bin` and `picoLoader9.bin` into `/_pico/`, over the official ones. It is optional, the launcher works the same with the official loader. It is the DSpico build, so on another flashcart keep the loader made for that card. Which games get the exit, and the known issues, are in the [release notes](../../releases/latest).

## Building from source
We recommend using WSL (Windows Subsystem for Linux), or MSYS2 to compile this repository.
The steps provided will assume you already have one of those environments set up.

1. Install [BlocksDS](https://blocksds.skylyrac.net/docs/setup/)
2. Fetch the submodules: `git submodule update --init`

## Compiling

1. Run `make`

Alternatively, build with Docker (the same image used by CI) without installing BlocksDS locally:

```sh
docker run --rm -v "$PWD":/work -w /work skylyrac/blocksds:slim-v1.16.0 make
```

The launcher can be found in the root directory under the name `LAUNCHER.nds`.

2. Copy `LAUNCHER.nds` to your SD card.
    - If you are using DSpico, rename to `_picoboot.nds` and place it in the root of your SD card.
3. Copy the `_pico` pico folder to the root of your SD card.

For DSpico the final directory structure will look like this:
```
.
├── _pico
│   ├── themes
│   │   ├── material
│   │   │   └── theme.json
│   │   └── raspberry
│   │       ├── bannerListCell.bin
│   │       ├── bannerListCellPltt.bin
│   │       ├── bannerListCellSelected.bin
│   │       ├── bannerListCellSelectedPltt.bin
│   │       ├── bottombg.bin
│   │       ├── gridcell.bin
│   │       ├── gridcellPltt.bin
│   │       ├── gridcellSelected.bin
│   │       ├── gridcellSelectedPltt.bin
│   │       ├── scrim.bin
│   │       ├── scrimPltt.bin
│   │       ├── theme.json
│   │       └── topbg.bin
│   ├── aplist.bin
│   ├── savelist.bin
│   ├── picoLoader7.bin
│   └── picoLoader9.bin
└── _picoboot.nds
```
Note: If you want to play DSiWare on the DSpico, additional files are required. See the [Pico Loader](https://github.com/LNH-team/pico-loader) readme for more information.

## Extra tools

The [`tools/`](tools/) directory contains desktop helper scripts for preparing SD card content: cover art converters and fetchers, banner and icon generators, and night background makers. See [Tools](docs/Tools.md).

## Data formats

The fork stores per-game data (favorites, launch counts, play time) in `/_pico/gamedata.json`. Each entry belongs to one ROM file. If a favorite or a play count is not where you expect it, [Data storage](docs/Enhanced.md#data-storage) explains why in a table. The file format itself is documented in [Game data](docs/GameData.md) for tool authors.

## Contributing

Bug reports, ideas and pull requests are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md).

Every change the fork makes on top of upstream is mapped in [The fork against upstream](docs/Upstream.md), with its commits and the upstream files it touches, so any of it can be taken on its own.

## License

Icons by [icons8](https://icons8.com/)

This project is licensed under the Zlib license. For details, see `LICENSE.txt`.

Additional licenses may apply to the project. For details, see the `license` directory.

## Contributors

Pico Launcher, by the LNH team:

- [@Gericom](https://github.com/Gericom)
- [@XLuma](https://github.com/XLuma)
- [@Dartz150](https://github.com/Dartz150)
- [@lifehackerhansol](https://github.com/lifehackerhansol)

All credit for the launcher's foundation goes to the LNH team. This fork only builds on their excellent work.

Pico Launcher Enhanced:

- [@rasalopa](https://github.com/rasalopa), who keeps the fork
- [@marlooonxdd](https://github.com/marlooonxdd): sleep when the lid closes ([#23](https://github.com/rasalopa/pico-launcher-enhanced/pull/23)) and a fix in the selector of the icon buttons ([#24](https://github.com/rasalopa/pico-launcher-enhanced/pull/24))
- [@BigBrainAFK](https://github.com/BigBrainAFK): save slots for DS games ([#32](https://github.com/rasalopa/pico-launcher-enhanced/pull/32))
