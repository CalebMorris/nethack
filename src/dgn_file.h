/* See LICENSE in the root of this project for change info */
#ifndef DGN_FILE_H
#define DGN_FILE_H

#include "align.h"

/*
 * Structures manipulated by the dungeon loader & compiler
 */

struct  couple {
        short   base, rand;
};

struct tmpdungeon {
        char    name[24],
                protoname[24];
        struct  couple  lev;
        int     flags,
                chance,
                levels,
                branches,
                entry_lev;              /* entry level for this dungeon */
        char    boneschar;
};

struct tmplevel {
        char    name[24];
        struct  couple  lev;
        int     chance, rndlevs, chain, flags;
        char    boneschar;
};

struct tmpbranch {
        char    name[24];       /* destination dungeon name */
        struct  couple  lev;
        int     chain;          /* index into tmplevel array (chained branch)*/
        int     type;           /* branch type (see below) */
        int     up;             /* branch is up or down */
};

/*
 *      Values for type for tmpbranch structure.
 */
enum {
    TBR_STAIR, /* connection with both ends having a staircase */
    TBR_NO_UP, /* connection with no up staircase */
    TBR_NO_DOWN, /* connection with no down staircase */
    TBR_PORTAL, /* portal connection */
};

/*
 *      Flags that map into the dungeon flags bitfields.
 */
enum {
    TOWN      = 1, /* levels only */
    HELLISH   = 2,
    MAZELIKE  = 4,
    ROGUELIKE = 8,
};

enum {
    D_ALIGN_NONE    = 0,
    D_ALIGN_CHAOTIC = (AM_CHAOTIC << 4),
    D_ALIGN_NEUTRAL = (AM_NEUTRAL << 4),
    D_ALIGN_LAWFUL  = (AM_LAWFUL << 4),
};

enum {
    D_ALIGN_MASK = 0x70,
};

/*
 *      Max number of prototype levels and branches.
 */
enum {
    LEV_LIMIT    = 50,
    BRANCH_LIMIT = 32,
};

#endif /* DGN_FILE_H */
