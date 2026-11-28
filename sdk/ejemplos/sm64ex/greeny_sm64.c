/*
 * Greeny achievements for sm64ex (Switch port). MIT license.
 *
 * Reads the save file and Mario's state twice a second and tells Greeny only what changed.
 * The achievements themselves (names, points, images) live in Greeny's catalog.
 * Without Greeny installed this does nothing.
 *
 * Hook it from src/pc/pc_main.c:
 *   greeny_sm64_begin();   after the game starts
 *   greeny_sm64_update();  once per frame, after game_loop_one_iteration()
 *   greeny_sm64_end();     in game_deinit()
 */
#define GREENY_IMPLEMENTATION
#include "greeny.h"

#include <ultra64.h>
#include "sm64.h"
#include "course_table.h"
#include "level_table.h"
#include "game/area.h"
#include "game/level_update.h"
#include "game/object_list_processor.h"
#include "game/save_file.h"

#define GREENY_SM64_ID "0500000000000001"

enum { S_STARS, S_COURSES, S_COIN_STARS, S_SECRET, S_TOADS, S_BOWSER1, S_BOWSER2,
       S_WING, S_METAL, S_VANISH, S_LIVES, S_MOAT, S_DOOR50, S_LOST_CAP, S_COUNT };

static const char *const sNames[S_COUNT] = {
    "stars", "courses_done", "coin_stars", "secret_stars", "toad_stars", "bowser_1", "bowser_2",
    "wing_cap", "metal_cap", "vanish_cap", "lives", "moat_drained", "door_50", "lost_cap",
};

/* Moves counted as events (each time Mario starts one). */
static const struct { u32 action; const char *name; } sMoves[] = {
    { ACT_TRIPLE_JUMP, "triple_jumps" }, { ACT_LONG_JUMP, "long_jumps" }, { ACT_WALL_KICK_AIR, "wall_kicks" },
    { ACT_BACKFLIP, "backflips" }, { ACT_GROUND_POUND, "ground_pounds" }, { ACT_LAVA_BOOST, "lava" },
};

static u32 sLast[S_COUNT];
static s16 sFile;          /* save file the values above belong to (0 = none yet) */
static s16 sLastCoins;
static s8 sLastLives = -100; /* -100 = not seen yet */
static u32 sLastAction;
static u32 sFrame;
static u8 sGrandStar, sEnding, sYoshi;

static u32 count_bits(u32 v) {
    u32 n = 0;
    for (; v; v &= v - 1) n++;
    return n;
}

void greeny_sm64_begin(void) {
    greeny_begin(GREENY_SM64_ID);
}

void greeny_sm64_end(void) {
    greeny_end();
}

/* Current value of every stat that lives in the save file (or in Mario). */
static void read_stats(u32 out[S_COUNT]) {
    s32 file = gCurrSaveFileNum - 1;
    u32 flags = save_file_get_flags();
    u32 courses = 0, coinStars = 0;

    for (s32 c = COURSE_MIN - 1; c < COURSE_STAGES_MAX; c++) { /* the 15 main courses */
        u32 f = save_file_get_star_flags(file, c);
        if ((f & 0x7F) == 0x7F) courses++;
        if (f & 0x40) coinStars++; /* star 7 = 100 coins */
    }
    out[S_STARS] = save_file_get_total_star_count(file, COURSE_MIN - 1, COURSE_MAX - 1);
    out[S_COURSES] = courses;
    out[S_COIN_STARS] = coinStars;
    /* bonus stages + castle secret stars (this call already adds the castle ones) */
    out[S_SECRET] = save_file_get_total_star_count(file, COURSE_BONUS_STAGES - 1, COURSE_MAX - 1);
    out[S_TOADS] = count_bits(save_file_get_star_flags(file, -1) & 0x07); /* 0x08 / 0x10 are MIPS */
    out[S_BOWSER1] = (flags & (SAVE_FLAG_HAVE_KEY_1 | SAVE_FLAG_UNLOCKED_BASEMENT_DOOR)) != 0;
    out[S_BOWSER2] = (flags & (SAVE_FLAG_HAVE_KEY_2 | SAVE_FLAG_UNLOCKED_UPSTAIRS_DOOR)) != 0;
    out[S_WING] = (flags & SAVE_FLAG_HAVE_WING_CAP) != 0;
    out[S_METAL] = (flags & SAVE_FLAG_HAVE_METAL_CAP) != 0;
    out[S_VANISH] = (flags & SAVE_FLAG_HAVE_VANISH_CAP) != 0;
    out[S_LIVES] = gMarioState->numLives > 0 ? (u32)gMarioState->numLives : 0;
    out[S_MOAT] = (flags & SAVE_FLAG_MOAT_DRAINED) != 0;
    out[S_DOOR50] = (flags & SAVE_FLAG_UNLOCKED_50_STAR_DOOR) != 0;
    /* the cap is somewhere else: blown off, or taken by Klepto, Ukiki or Mr. Blizzard */
    out[S_LOST_CAP] = (flags & (SAVE_FLAG_CAP_ON_GROUND | SAVE_FLAG_CAP_ON_KLEPTO | SAVE_FLAG_CAP_ON_UKIKI | SAVE_FLAG_CAP_ON_MR_BLIZZARD)) != 0;
}

void greeny_sm64_update(void) {
    /* the ending (cake) screen has no Mario, so it is checked first */
    u8 ending = gCurrLevelNum == LEVEL_ENDING;
    if (ending && !sEnding) greeny_event("ending");
    sEnding = ending;

    /* the rest only while Mario is in a level (not on the title or file select screens) */
    if (gMarioObject == NULL || gCurrSaveFileNum < 1) return;

    /* coins: every frame, so no coin is missed. numCoins goes back to 0 in each new level. */
    s16 coins = gMarioState->numCoins;
    if (coins > sLastCoins && sFile == gCurrSaveFileNum) greeny_add("coins_total", (u32)(coins - sLastCoins));
    sLastCoins = coins;

    /* one-off moments that the save file doesn't keep */
    u8 grand = gMarioState->action == ACT_JUMBO_STAR_CUTSCENE;
    if (grand && !sGrandStar) greeny_event("bowser_3");
    sGrandStar = grand;
    /* deaths: a death takes exactly one life (other drops are a reset, like choosing the file again);
     * going below 0 is a game over */
    s8 lives = gMarioState->numLives;
    if (sLastLives != -100 && sFile == gCurrSaveFileNum && lives == sLastLives - 1) {
        greeny_event("deaths");
        if (lives < 0) greeny_event("game_over");
    }
    sLastLives = lives;

    /* moves: counted when Mario starts them */
    u32 action = gMarioState->action;
    if (action != sLastAction)
        for (unsigned i = 0; i < sizeof(sMoves) / sizeof(sMoves[0]); i++)
            if (action == sMoves[i].action) greeny_event(sMoves[i].name);
    sLastAction = action;

    u8 yoshi = gSpecialTripleJump != 0; /* only Yoshi on the roof turns this on */
    if (yoshi && !sYoshi) greeny_event("yoshi");
    sYoshi = yoshi;

    if (++sFrame % 15) return; /* the rest, twice a second */

    u32 now[S_COUNT];
    read_stats(now);
    /* another save file: send everything again, so what it already had counts as "already had" */
    bool all = sFile != gCurrSaveFileNum;
    for (int i = 0; i < S_COUNT; i++) {
        if (all || now[i] != sLast[i]) greeny_stat(sNames[i], now[i]);
        sLast[i] = now[i];
    }
    sFile = gCurrSaveFileNum;
}
