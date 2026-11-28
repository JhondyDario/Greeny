/*
 * Greeny SDK: achievements for homebrew games and ports on Nintendo Switch.
 * https://github.com/JhondyDario/Greeny
 *
 * MIT License. Copyright (c) 2026 Jhondy. See LICENSE next to this file.
 *
 * One file, no dependencies besides the C standard library.
 *
 * Your game does not decide the achievements: it only reports what happens (stats and events).
 * Which achievements exist, their names, points and pictures live in Greeny's catalog, like the ones
 * for retail games. That way they can be tuned without rebuilding your game.
 *
 * Usage:
 *     #define GREENY_IMPLEMENTATION      // in ONE .c file of your project
 *     #include "greeny.h"
 *
 *     greeny_begin("0500000000000001");  // the id Greeny gives your game
 *     greeny_stat("stars", 70);          // current value of a stat
 *     greeny_event("bowser_1");          // something happened (adds 1)
 *     greeny_add("coins_total", 5);      // adds N (coins, enemies...)
 *     greeny_end();                      // when the game closes
 *
 * Without Greeny installed, all of this does nothing and never fails.
 */
#ifndef GREENY_SDK_H
#define GREENY_SDK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GREENY_SDK_VERSION 1

/* Starts the session. `port_id`: 16 hex digits assigned by Greeny (they start with 05).
 * Returns false if Greeny is not installed (then the other calls do nothing). */
bool greeny_begin(const char *port_id);

/* CURRENT value of a stat: stars, coins, lives... Call it when it changes and, when a save file is
 * loaded, with every value from the save (so what the player already had counts as "already had",
 * without points). Use this one, not greeny_event, for everything that lives in the save file.
 * `name`: lowercase letters, digits and _ (1 to 31 characters). */
void greeny_stat(const char *name, uint32_t value);

/* Something that happens in the session and is not in the save file: adds 1 to that stat. */
void greeny_event(const char *name);

/* Like greeny_event, but adds `amount` (for example 5 coins at once). 0 does nothing. */
void greeny_add(const char *name, uint32_t amount);

/* Ends the session (when going back to the homebrew menu). */
void greeny_end(void);

#ifdef __cplusplus
}
#endif

#ifdef GREENY_IMPLEMENTATION
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifndef GREENY_ROOT /* only for tests on PC */
#define GREENY_ROOT "sdmc:/config/greeny"
#endif
#define GREENY__DIR GREENY_ROOT
#define GREENY__INBOX GREENY__DIR "/sdk"

static char greeny__file[256];
static bool greeny__on;

static bool greeny__valid(const char *s, size_t max) {
    size_t n = 0;
    for (; s && s[n]; n++)
        if (!((s[n] >= 'a' && s[n] <= 'z') || (s[n] >= '0' && s[n] <= '9') || s[n] == '_')) return false;
    return n > 0 && n <= max;
}

static void greeny__write(const char *verb, const char *name, uint32_t value) {
    if (!greeny__on || !greeny__valid(name, 31)) return;
    /* one whole line (64 bytes max) written at once, and the file is closed right away:
     * Greeny only reads a file it could rename, so it never sees half a line */
    char line[64];
    int n = snprintf(line, sizeof(line), "%s %s %lu\n", verb, name, (unsigned long)value);
    if (n <= 0 || n >= (int)sizeof(line)) return;
    FILE *f = fopen(greeny__file, "a");
    if (!f) return;
    fwrite(line, 1, (size_t)n, f);
    fclose(f);
}

bool greeny_begin(const char *port_id) {
    struct stat st;
    greeny__on = false;
    if (!port_id || strlen(port_id) != 16 || stat(GREENY__DIR, &st) != 0) return false; /* no Greeny: nothing */
    for (int i = 0; i < 16; i++) {
        char c = port_id[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) return false;
    }
    mkdir(GREENY__INBOX, 0777);
    snprintf(greeny__file, sizeof(greeny__file), GREENY__INBOX "/%.16s.txt", port_id);
    greeny__on = true;
    greeny__write("begin", "session", GREENY_SDK_VERSION);
    return true;
}

void greeny_stat(const char *name, uint32_t value) { greeny__write("set", name, value); }
void greeny_event(const char *name) { greeny__write("add", name, 1); }
void greeny_add(const char *name, uint32_t amount) { if (amount) greeny__write("add", name, amount); }
void greeny_end(void) { greeny__write("end", "session", 0); greeny__on = false; }
#endif /* GREENY_IMPLEMENTATION */

#endif /* GREENY_SDK_H */
