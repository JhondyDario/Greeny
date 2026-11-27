# Greeny Achievements — Technical FAQ

Answers to the questions we get most often on Discord and in forums.

## How much RAM does it use?

Very little. The part that runs in the background, while you play, uses **about 1.5 MB** of system memory in
total (code plus memory). It takes nothing from the game's own memory.

- The **Greeny app** only uses memory while it is open, like any other homebrew app.
- The **Tesla overlay** only uses memory while Tesla is open.

There is no noticeable effect on performance or battery. Greeny only reads a few values a few times per
second.

## Is Greeny open source?

Not fully. **Greeny is free, with open integration formats.** The core is closed to protect fair rankings: if
the achievement definitions and the code were public, faking achievements would be trivial. What is open:

- the **file formats** any homebrew can read to show Greeny achievements ([INTEGRATIONS.md](INTEGRATIONS.md));
- the **Tesla overlay**, open source under GPL-2.0 (its source ships with every release);
- the list of **third-party components** we use ([THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)).

Soon an **open-source SDK** will let homebrew and ports add their own achievements.

## Does it track achievements in homebrew (.nro) games?

Not yet. Greeny is built around retail Switch games.

We are looking at a simple **integration for homebrew developers**, so a homebrew game or port can report its
own achievements to Greeny. If you make homebrew and want this, tell us on
[Discord](https://discord.gg/4KU8C9bEHy).

## Can achievements unlock from my save file?

No, and that is on purpose. Achievements unlock **live, while you play**, the moment you do the thing.
Unlocking them by scanning a save file would hand you everything at once and take the fun out of earning them.

If you already met a goal before Greeny started watching (for example, you already had 99 lives), you still get
the achievement and its pop-up. It is marked as *already had*, though, and gives **no points** and does not
count for rankings.

## Does it work on emulators (Ryujinx, Eden, Citron…)?

Not yet. It only runs on a real console for now. PC emulator support is on the roadmap, but there is no date.

## Does it work on Switch 2?

Greeny runs wherever **Atmosphère** runs. Today that means the original Switch family (Switch, Switch Lite,
OLED). When homebrew becomes possible on Switch 2, we will look into it.

## Online achievements with Nextendo Network?

It is planned: achievements for playing online that are compatible with Nextendo Network. There is no date
yet; announcements will be on Discord and on the website.

## Is it safe? Can my console get banned?

Greeny does not modify your games or your saves: it only **reads** a few values while you play. The usual
custom firmware advice still applies, as it does for any homebrew:

- use **emuMMC**,
- keep your custom firmware **offline from Nintendo's servers**,
- only download Greeny from the
  [official releases](https://github.com/JhondyDario/Greeny/releases/latest).

## Which games are supported?

The full, always up-to-date list is on the website's
[Games](https://jhondydario.github.io/Greeny/#/juegos) page. Each game shows its status: *Not started*,
*Testing*, *Playable* or *Perfect*. New games arrive through the catalog, with no app update needed.

## A game has achievements but none unlock

Achievements are made for **one version of each game**. If yours is different, Greeny tells you when the
game starts ("Achievements off"). Update the game to the version shown on its page on the website.

If the version matches and something still does not unlock, report the achievement from the app or the
website. We look at every report.

## I want my homebrew or launcher to show Greeny achievements

Read **[INTEGRATIONS.md](INTEGRATIONS.md)**. It lists the files you can read and what credit to give.
