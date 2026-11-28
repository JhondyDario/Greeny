# Greeny SDK

Add Greeny achievements to your homebrew game or port in about 5 minutes. Players get the same pop-ups,
app, profile and website as with retail games.

- **One file**: [`greeny.h`](greeny.h), plain C, no dependencies. Works from C and C++.
- **MIT license**: use it in any project, open or closed.
- **Safe**: without Greeny installed, every call does nothing.

## 1. Get an id

Tell us about your game on [Discord](https://discord.gg/4KU8C9bEHy) or in a
[GitHub issue](https://github.com/JhondyDario/Greeny/issues). You get a 16-digit id that starts with
`05`, for example `0500000000000001`.

## 2. Add the calls

```c
#define GREENY_IMPLEMENTATION   // in ONE .c file
#include "greeny.h"

int main(void) {
    greeny_begin("0500000000000001");
    ...
    // when a save file is loaded: send everything it has
    greeny_stat("stars", save.stars);
    greeny_stat("boss_1", save.boss_1_beaten);

    // while playing, when something changes
    greeny_stat("stars", save.stars);   // a value from the save file
    greeny_event("secret_room");        // something that happened (+1)
    greeny_add("coins_total", 5);       // +N
    ...
    greeny_end();
}
```

| Call | Use it for |
|---|---|
| `greeny_stat(name, value)` | The **current value** of anything that lives in the save file: stars, levels, bosses, items. |
| `greeny_event(name)` | Something that happened and isn't in the save file. Adds 1. |
| `greeny_add(name, n)` | The same, adding `n`. |

Names use lowercase letters, digits and `_` (up to 31 characters). Send a stat when it **changes**, not
every frame.

## 3. Decide the achievements with us

Send us the list of stats your game reports and the achievements you'd like ("collect 70 stars",
"beat the final boss"). We add them to Greeny's catalog with their names, points and pictures. They can be
changed later without rebuilding your game.

## Rules

- **Live, not from the save.** What the save file already had when the game starts unlocks as *already
  had*, without points. That's why you send the save's values right when it loads.
- **Play time is automatic.** Greeny knows how long your game is open; there's nothing to send.
- **Fair play.** Any homebrew can talk to Greeny, so achievements from homebrew and ports **don't add
  points to the global ranking**. They show on profiles with a **Port** tag and have their own ranking.
- Requires Greeny **1.0.2** or newer on the console.
- Your game must run **as an application** (hold R while starting a game, or a forwarder on the HOME menu),
  not from the Album. That's also where homebrew games get the most memory.

## Example: sm64ex

[`ejemplos/sm64ex`](ejemplos/sm64ex) adds 46 achievements to the Switch port of sm64ex. The game itself
only changes in 3 lines; everything else is one extra file.

## License

The SDK (this folder) is under the [MIT license](LICENSE). The rest of Greeny keeps its own license.
