/* See LICENSE in the root of this project for change info */
#ifndef ARTIFACT_H
#define ARTIFACT_H

#include "align.h"
#include "permonst.h"

enum {
    SPFX_NONE   = 0x0000000L,  /* no special effects, just a bonus */
    SPFX_NOGEN  = 0x0000001L,  /* item is special, bequeathed by gods */
    SPFX_RESTR  = 0x0000002L,  /* item is restricted - can't be named */
    SPFX_INTEL  = 0x0000004L,  /* item is self-willed - intelligent */
    SPFX_SPEAK  = 0x0000008L,  /* item can speak (not implemented) */
    SPFX_SEEK   = 0x0000010L,  /* item helps you search for things */
    SPFX_WARN   = 0x0000020L,  /* item warns you of danger */
    SPFX_ATTK   = 0x0000040L,  /* item has a special attack (attk) */
    SPFX_DEFN   = 0x0000080L,  /* item has a special defence (defn) */
    SPFX_DRLI   = 0x0000100L,  /* drains a level from monsters */
    SPFX_SEARCH = 0x0000200L,  /* helps searching */
    SPFX_BEHEAD = 0x0000400L,  /* beheads monsters */
    SPFX_HALRES = 0x0000800L,  /* blocks hallucinations */
    SPFX_ESP    = 0x0001000L,  /* ESP (like amulet of ESP) */
    SPFX_STLTH  = 0x0002000L,  /* Stealth */
    SPFX_REGEN  = 0x0004000L,  /* Regeneration */
    SPFX_EREGEN = 0x0008000L,  /* Energy Regeneration */
    SPFX_HSPDAM = 0x0010000L,  /* 1/2 spell damage (on player) in combat */
    SPFX_HPHDAM = 0x0020000L,  /* 1/2 physical damage (on player) in combat */
    SPFX_TCTRL  = 0x0040000L,  /* Teleportation Control */
    SPFX_LUCK   = 0x0080000L,  /* Increase Luck (like Luckstone) */
    SPFX_DMONS  = 0x0100000L,  /* attack bonus on one monster type */
    SPFX_DCLAS  = 0x0200000L,  /* attack bonus on monsters w/ symbol mtype */
    SPFX_DFLAG1 = 0x0400000L,  /* attack bonus on monsters w/ mflags1 flag */
    SPFX_DFLAG2 = 0x0800000L,  /* attack bonus on monsters w/ mflags2 flag */
    SPFX_DALIGN = 0x1000000L,  /* attack bonus on non-aligned monsters  */
    SPFX_DBONUS = 0x1F00000L,  /* attack bonus mask */
    SPFX_XRAY   = 0x2000000L,  /* gives X-RAY vision to player */
    SPFX_REFLECT= 0x4000000L, /* Reflection */
};


struct artifact {
    short       otyp;
    const char  *name;
    unsigned long spfx;     /* special effect from wielding/wearing */
    unsigned long cspfx;    /* special effect just from carrying obj */
    unsigned long mtype;    /* monster type, symbol, or flag */
    struct attack attk, defn, cary;
    unsigned char       inv_prop;   /* property obtained by invoking artifact */
    aligntyp    alignment;  /* alignment of bequeathing gods */
    short       role;       /* character role associated with */
    short       race;       /* character race associated with */
    long        cost;       /* price when sold to hero (default 100 x base cost) */
};

/* invoked properties with special powers */
enum {
    TAMING = (LAST_PROP+1),
    HEALING,
    ENERGY_BOOST,
    UNTRAP,
    CHARGE_OBJ,
    LEV_TELE,
    CREATE_PORTAL,
    ENLIGHTENING,
    CREATE_AMMO,
};

#endif /* ARTIFACT_H */
