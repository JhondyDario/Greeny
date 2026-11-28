# Greeny achievements for sm64ex (Switch)

The first port with Greeny achievements, and the example for the Greeny SDK. It is the Nintendo Switch port
of [sm64ex](https://github.com/fgsfdsfgs/sm64ex/tree/switch) with **46 achievements**: stars, Bowser fights,
caps, full courses, 100-coin stars, secret stars, coins, lives, deaths, jumps, lava and play time.

## What changes in the game

Very little:

| File | Change |
|---|---|
| `src/pc/greeny.h` | The Greeny SDK (one file, MIT). |
| `src/pc/greeny_sm64.c` | Reads the save file and Mario's state and tells Greeny what changed. |
| `src/pc/pc_main.c` | 3 calls: start, once per frame, end. |
| `Makefile` | Finds SDL2 and GLES on current devkitPro images. |

The port never decides which achievements exist. It only reports values like `stars = 70` or
`bowser_3 + 1`. The achievements themselves (names, points, pictures) live in Greeny's catalog, so they can
change without rebuilding the game. Without Greeny installed, the port works exactly as before.

## Build it

**Easiest (Windows):** download **Greeny-SM64-Builder.zip** from the
[releases](https://github.com/JhondyDario/greeny-sm64/releases/latest), double-click `Crear port.bat` and pick
your ROM. It converts and checks the ROM, builds the port with Docker Desktop and leaves the `.nro` ready.

**By hand:**

You need Docker and **your own** dump of the game (US version). The ROM is only used on your machine to
extract the assets; nothing from it is in this repository, and please don't ask for one.

The ROM must be the US version in `.z64` format (SHA-1 `9bef1128717f958171a4afac3ed78ee2bb4e86ce`).

```bash
./compilar.sh /path/to/baserom.us.z64
```

Or build the ready-made branch, which already has all the changes:
[JhondyDario/greeny-sm64](https://github.com/JhondyDario/greeny-sm64/tree/greeny) (branch `greeny`). Put
`baserom.us.z64` in its folder and run:

```bash
docker run --rm -v "${PWD}:/src" -w /src devkitpro/devkita64:latest make TARGET_SWITCH=1 -j4
```

Copy `build/us_nx/sm64.us.f3dex2e.nro` to `sdmc:/switch/`. Open it **as an application**: hold **R** while
starting any game, then pick it in the homebrew menu, or install it as a **forwarder** (its own icon on the
HOME menu), which also runs it as an application. Opened from the Album it gets less memory and Greeny
does not see it. You need Greeny 1.0.2 or newer.

## Fair play

Any homebrew can talk to Greeny, so achievements from ports and homebrew **don't add points to the global
ranking**. They show on your profile with a **Port** tag and have their own per-game ranking.
What your save file already had when you start counts as *already had*, like in any other game.
