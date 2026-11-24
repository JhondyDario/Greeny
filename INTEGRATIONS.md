# Integrating Greeny

Greeny is meant to grow beyond its own app. Like RetroAchievements is built into many emulators
while staying one project, other homebrew (HOME menus, launchers, overlays, dashboards) is welcome to
show Greeny achievements — as long as **Greeny stays the source** and gets the credit.

## What you can do, without asking

- **Read the files Greeny writes on the SD card** (listed below) to show a player's achievements,
  progress, points or recently played games inside your project.
- **Launch the Greeny app** from your project and **link to the official download**:
  <https://github.com/JhondyDario/Greeny/releases/latest>
- **Use the Greeny name and logo** to label that feature, next to the credit below.

## What you need to do

- Show a visible credit wherever Greeny data appears: **"Achievements by Greeny"**, with a link to
  <https://github.com/JhondyDario/Greeny> where links are possible.
- Point players to the official Greeny release to install or update it.

## What is not allowed

- Copying, converting, decrypting or redistributing Greeny's **achievement definitions** (`.grx` files or
  their contents), or using them to build another achievement system.
- Bundling or redistributing Greeny's **binaries** (app, background service, overlay) or its source code.
- Writing to Greeny's files, or presenting Greeny achievements as your own system.

Anything beyond this (bundling, a deeper integration, commercial use) needs written permission:
open an issue or ask on [Discord](https://discord.gg/4KU8C9bEHy).

## Files you can read

Everything lives in `sdmc:/config/greeny/`. Title IDs are 16 uppercase hex digits. Treat every file as
read-only and expect it to be missing (Greeny may not be installed, or the game not played yet).

| File | Contents |
|---|---|
| `unlocked/<TITLE_ID>.txt` | One line per unlocked achievement: `id\|unix_time\|verified`. `verified` is `1` when Greeny saw it happen while playing, `0` if it was already met when the game opened (it gives no points). |
| `progress/<TITLE_ID>.txt` | Progress of locked achievements: `id value` per line (e.g. `coins_100 37`). |
| `played/<TITLE_ID>.txt` | Unix time when the game was last opened with Greeny. |
| `info/<TITLE_ID>.txt` | The game's achievement list for display: `[id]` sections with `name`, `desc`, `name.en`, `desc.en`, `points`, `tier` (`bronce`, `plata` or `oro`; when missing it comes from the points). It has no memory addresses. |
| `config.ini` | Greeny's settings (`lang = es/en/auto`, `theme`, …). |

Points per achievement and medals: bronze up to 10 points, silver up to 25, gold above (unless `tier` says
otherwise). The website (<https://jhondydario.github.io/Greeny/>) has public profiles and rankings that
you can link to: `https://jhondydario.github.io/Greeny/#/u/<username>`.

The file formats may change between versions; this page is updated when they do.
