/* See LICENSE in the root of this project for change info */
#ifndef MONATTK_H
#define MONATTK_H

/*      Add new attack types below - ordering affects experience (exper.c).
 *      Attacks > AT_BUTT are worth extra experience.
 */
enum {
    AT_ANY  = (-1), /* fake attack; dmgtype_fromattack wildcard */
    AT_NONE = 0, /* passive monster (ex. acid blob) */
    AT_CLAW, /* claw (punch, hit, etc.) */
    AT_BITE, /* bite */
    AT_KICK, /* kick */
    AT_BUTT, /* head butt (ex. a unicorn) */
    AT_TUCH, /* touches */
    AT_STNG, /* sting */
    AT_HUGS, /* crushing bearhug */

    AT_SPIT = 10, /* spits substance - ranged */
    AT_ENGL, /* engulf (swallow or by a cloud) */
    AT_BREA, /* breath - ranged */
    AT_EXPL, /* explodes - proximity */
    AT_BOOM, /* explodes when killed */
    AT_GAZE, /* gaze - ranged */
    AT_TENT, /* tentacles */
};

enum {
    AT_WEAP = 254, /* uses weapon */
    AT_MAGC = 255, /* uses magic spell(s) */
};

/*      Add new damage types below.
 *
 *      Note that 1-10 correspond to the types of attack used in buzz().
 *      Please don't disturb the order unless you rewrite the buzz() code.
 */
enum {
    AD_ANY  = (-1)    /* fake damage; attacktype_fordmg wildcard */
    AD_PHYS = 0, /* ordinary physical */
    AD_MAGM, /* magic missiles */
    AD_FIRE, /* fire damage */
    AD_COLD, /* frost damage */
    AD_SLEE, /* sleep ray */
    AD_DISN, /* disintegration (death ray) */
    AD_ELEC, /* shock damage */
    AD_DRST, /* drains str (poison) */
    AD_ACID, /* acid damage */
    AD_SPC1, /* for extension of buzz() */
    AD_SPC2, /* for extension of buzz() */
    AD_BLND, /* blinds (yellow light) */
    AD_STUN, /* stuns */
    AD_SLOW, /* slows */
    AD_PLYS, /* paralyses */
    AD_DRLI, /* drains life levels (Vampire) */
    AD_DREN, /* drains magic energy */
    AD_LEGS, /* damages legs (xan) */
    AD_STON, /* petrifies (Medusa, cockatrice) */
    AD_STCK, /* sticks to you (mimic) */
    AD_SGLD, /* steals gold (leppie) */
    AD_SITM, /* steals item (nymphs) */
    AD_SEDU, /* seduces & steals multiple items */
    AD_TLPT, /* teleports you (Quantum Mech.) */
    AD_RUST, /* rusts armour (Rust Monster)*/
    AD_CONF, /* confuses (Umber Hulk) */
    AD_DGST, /* digests opponent (trapper, etc.) */
    AD_HEAL, /* heals opponent's wounds (nurse) */
    AD_WRAP, /* special "stick" for eels */
    AD_WERE, /* confers lycanthropy */
    AD_DRDX, /* drains dexterity (quasit) */
    AD_DRCO, /* drains constitution */
    AD_DRIN, /* drains intelligence (mind flayer) */
    AD_DISE, /* confers diseases */
    AD_DCAY, /* decays organics (brown Pudding) */
    AD_SSEX, /* Succubus seduction (extended) */
    AD_HALU, /* causes hallucination */
    AD_DETH, /* for Death only */
    AD_PEST, /* for Pestilence only */
    AD_FAMN, /* for Famine only */
    AD_SLIM, /* turns you into green slime */
    AD_ENCH, /* remove enchantment (disenchanter) */
    AD_CORR, /* corrode armor (black pudding) */
};

enum {
    AD_CLRC = 240     /* random clerical spell */
    AD_SPEL, /* random magic spell */
    AD_RBRE, /* random breath weapon */
}

enum {
    AD_SAMU = 252, /* hits, may steal Amulet (Wizard) */
    AD_CURS, /* random curse (ex. gremlin) */
};

/*
 *  Monster to monster attacks.  When a monster attacks another (mattackm),
 *  any or all of the following can be returned.  See mattackm() for more
 *  details.
 */
enum {
    MM_MISS     = 0x0, /* aggressor missed */
    MM_HIT      = 0x1, /* aggressor hit defender */
    MM_DEF_DIED = 0x2, /* defender died */
    MM_AGR_DIED = 0x4, /* aggressor died */
};

#endif /* MONATTK_H */
