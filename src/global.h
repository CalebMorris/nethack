/* See LICENSE in the root of this project for change info */

#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>


/* Files expected to exist in the playground directory. */
static const char* RECORD = "run/record"; /* file containing list of topscorers */
static const char* HELP = "help" /* file containing command descriptions */
static const char* SHELP = "hh" /* abbreviated form of the same */
static const char* RUMORFILE = "rumors" /* file with fortune cookies */
static const char* ORACLEFILE = "oracles" /* file with oracular information */
static const char* DATAFILE = "data" /* file giving the meaning of symbols used */
static const char* CMDHELPFILE = "cmdhelp" /* file telling what commands do */
static const char* HISTORY = "history" /* file giving nethack's history */
static const char* LICENSE = "license" /* file with license information */
static const char* OPTIONFILE = "opthelp" /* file explaining runtime options */
static const char* OPTIONS_USED = "options" /* compile-time options, for #version */

static const char* LEV_EXT = ".lev" /* extension for special level files */


#define strcmpi(a,b) strncmpi((a),(b),-1)

#define SIZE(x) (int)(sizeof(x) / sizeof(x[0]))


/* A limit for some NetHack int variables.  It need not, and for comparable
 * scoring should not, depend on the actual limit on integers for a
 * particular machine, although it is set to the minimum required maximum
 * signed integer for C (2^15 -1).
 */
static const int LARGEST_INT = 32767;

/* Used for consistency checks of various data files; */
struct version_info {
    unsigned long   incarnation;    /* actual version number */
    unsigned long   feature_set;    /* bitmask of config settings */
    unsigned long   entity_count;   /* # of monsters and objects */
    unsigned long   struct_sizes;   /* size of key structs */
};


/*
 * Configurable internal parameters.
 *
 * Please be very careful if you are going to change one of these.  Any
 * changes in these parameters, unless properly done, can render the
 * executable inoperative.
 */

/* size of terminal screen is (at least) (ROWNO+3) by COLNO */
enum {
    COLNO = 80,
    ROWNO = 21,

    MAXNROFROOMS = 40, /* max number of rooms per level */
    MAX_SUBROOMS = 24, /* max # of subrooms in a given room */
    DOORMAX = 120, /* max number of doors per level */
    BUFSZ = 256, /* for getlin buffers */

    QBUFSZ = 128, /* for building question text */
    TBUFSZ = 300, /* toplines[] buffer max msg: 3 81char names */
                  /* plus longest prefix plus a few extra words */

    PL_NSIZ = 32, /* name of player, ghost, shopkeeper */
    PL_CSIZ = 32, /* sizeof pl_character */
    PL_FSIZ = 32, /* fruit name */
    PL_PSIZ = 63, /* player-given names for pets, other monsters, objects */

    MAXDUNGEON = 16, /* current maximum number of dungeons */
    MAXLEVEL = 32, /* max number of levels in one dungeon */
    MAXSTAIRS = 1, /* max # of special stairways in a dungeon */
    ALIGNWEIGHT = 4, /* generation weight of alignment */

    MAXULEV = 30, /* max character experience level */

    MAXMONNO = 120, /* extinct monst after this number created */
    MHPMAX = 500, /* maximum monster hp */
};

#endif /* GLOBAL_H */
