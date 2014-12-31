/* See LICENSE in the root of this project for change info */

#include "hack.h"
#include "pm_props.h"
#include "extern.h"
#include "display.h"
#include "do_name.h"
#include "objnam.h"
#include "dbridge.h"
#include "shk.h"
#include "priest.h"
#include "mhitm.h"
#include "invent.h"

/* Disintegration rays have special treatment; corpses are never left.
 * But the routine which calculates the damage is separate from the routine
 * which kills the monster.  The damage routine returns this cookie to
 * indicate that the monster should be disintegrated.
 */
#define MAGIC_COOKIE 1000

static bool obj_zapped;
static int poly_zapped;

extern bool notonhead;       /* for long worms */

/* kludge to use mondied instead of killed */
extern bool m_using;

static void costly_cancel(struct obj *);
static void polyuse(struct obj*, int, int);
static void create_polymon(struct obj *, int);
static bool zap_updown(struct obj *);
static int zhitm(struct monst *,int,int,struct obj **);
static void zhitu(int,int,const char *,signed char,signed char);
static void revive_egg(struct obj *);
static bool zap_steed(struct obj *);

static int zap_hit(int,int);
static void backfire(struct obj *);
static int spell_hit_bonus(int);

#define ZT_MAGIC_MISSILE        (AD_MAGM-1)
#define ZT_FIRE                 (AD_FIRE-1)
#define ZT_COLD                 (AD_COLD-1)
#define ZT_SLEEP                (AD_SLEE-1)
#define ZT_DEATH                (AD_DISN-1)     /* or disintegration */
#define ZT_LIGHTNING            (AD_ELEC-1)
#define ZT_POISON_GAS           (AD_DRST-1)
#define ZT_ACID                 (AD_ACID-1)
/* 8 and 9 are currently unassigned */

#define ZT_WAND(x)              (x)
#define ZT_SPELL(x)             (10+(x))
#define ZT_BREATH(x)            (20+(x))

#define is_hero_spell(type)     ((type) >= 10 && (type) < 20)

static const char are_blinded_by_the_flash[] = "are blinded by the flash!";

/* also used in buzzmu(mcastu.c) */
const char * const flash_types[] = {
        "magic missile",        /* Wands must be 0-9 */
        "bolt of fire",
        "bolt of cold",
        "sleep ray",
        "death ray",
        "bolt of lightning",
        "",
        "",
        "",
        "",

        "magic missile",        /* Spell equivalents must be 10-19 */
        "fireball",
        "cone of cold",
        "sleep ray",
        "finger of death",
        "bolt of lightning",    /* There is no spell, used for retribution */
        "",
        "",
        "",
        "",

        "blast of missiles",    /* Dragon breath equivalents 20-29*/
        "blast of fire",
        "blast of frost",
        "blast of sleep gas",
        "blast of disintegration",
        "blast of lightning",
        "blast of poison gas",
        "blast of acid",
        "",
        ""
};

/* Routines for IMMEDIATE wands and spells. */
/* bhitm: monster mtmp was hit by the effect of wand or spell otmp */
int bhitm(struct monst *mtmp, struct obj *otmp) {
    bool wake = true; /* Most 'zaps' should wake monster */
    bool reveal_invis = false;
    bool dbldam = Role_if(PM_KNIGHT) && u.uhave.questart;
    int dmg, otyp = otmp->otyp;
    const char *zap_type_text = "spell";
    struct obj *obj;
    bool disguised_mimic = (mtmp->data->mlet == S_MIMIC && mtmp->m_ap_type != M_AP_NOTHING);

    if (u.uswallow && mtmp == u.ustuck)
        reveal_invis = false;

    switch (otyp) {
        case WAN_STRIKING:
            zap_type_text = "wand";
            /* fall through */
        case SPE_FORCE_BOLT:
            reveal_invis = true;
            if (resists_magm(mtmp)) { /* match effect on player */
                shieldeff(mtmp->mx, mtmp->my);
                break; /* skip makeknown */
            } else if (u.uswallow || rnd(20) < 10 + find_mac(mtmp)) {
                dmg = d(2, 12);
                if (dbldam)
                    dmg *= 2;
                if (otyp == SPE_FORCE_BOLT)
                    dmg += spell_damage_bonus();
                hit(zap_type_text, mtmp, exclam(dmg));
                (void)resist(mtmp, otmp->oclass, dmg, TELL);
            } else
                miss(zap_type_text, mtmp);
            makeknown(otyp);
            break;
        case WAN_SLOW_MONSTER:
        case SPE_SLOW_MONSTER:
            if (!resist(mtmp, otmp->oclass, 0, NOTELL)) {
                mon_adjust_speed(mtmp, -1, otmp);
                m_dowear(mtmp, false); /* might want speed boots */
                if (u.uswallow && (mtmp == u.ustuck) && is_whirly(mtmp->data)) {
                    message_monster(MSG_YOU_DISTRUPT_ENGULFER, mtmp);
                    message_const(MSG_A_HUGE_HOLE_OPENS_UP);
                    expels(mtmp, mtmp->data, true);
                }
            }
            break;
        case WAN_SPEED_MONSTER:
            if (!resist(mtmp, otmp->oclass, 0, NOTELL)) {
                mon_adjust_speed(mtmp, 1, otmp);
                m_dowear(mtmp, false); /* might want speed boots */
            }
            break;
        case WAN_UNDEAD_TURNING:
        case SPE_TURN_UNDEAD:
            wake = false;
            if (unturn_dead(mtmp))
                wake = true;
            if (is_undead(mtmp->data)) {
                reveal_invis = true;
                wake = true;
                dmg = rnd(8);
                if (dbldam)
                    dmg *= 2;
                if (otyp == SPE_TURN_UNDEAD)
                    dmg += spell_damage_bonus();
                flags.bypasses = true; /* for make_corpse() */
                if (!resist(mtmp, otmp->oclass, dmg, NOTELL)) {
                    if (mtmp->mhp > 0)
                        monflee(mtmp, 0, false, true);
                }
            }
            break;
        case WAN_POLYMORPH:
        case SPE_POLYMORPH:
        case POT_POLYMORPH:
            if (resists_magm(mtmp)) {
                /* magic resistance protects from polymorph traps, so make
                 it guard against involuntary polymorph attacks too... */
                shieldeff(mtmp->mx, mtmp->my);
            } else if (!resist(mtmp, otmp->oclass, 0, NOTELL)) {
                /* natural shapechangers aren't affected by system shock
                 (unless protection from shapechangers is interfering
                 with their metabolism...) */
                if (mtmp->cham == CHAM_ORDINARY && !rn2(25)) {
                    if (canseemon(mtmp)) {
                        monster_message(MSG_FAILED_POLYMORPH, mtmp);
                        makeknown(otyp);
                    }
                    /* dropped inventory shouldn't be hit by this zap */
                    for (obj = mtmp->minvent; obj; obj = obj->nobj)
                        bypass_obj(obj);
                    /* flags.bypasses = true; ## for make_corpse() */
                    /* no corpse after system shock */
                    xkilled(mtmp, 3);
                } else if (newcham(mtmp, (struct permonst *)0, (otyp != POT_POLYMORPH), false)) {
                    if (!Hallucination() && canspotmon(mtmp))
                        makeknown(otyp);
                }
            }
            break;
        case WAN_CANCELLATION:
        case SPE_CANCELLATION:
            (void)cancel_monst(mtmp, otmp, true, true, false);
            break;
        case WAN_TELEPORTATION:
        case SPE_TELEPORT_AWAY:
            reveal_invis = !u_teleport_mon(mtmp, true);
            break;
        case WAN_MAKE_INVISIBLE: {
            int oldinvis = mtmp->minvis;
            mon_set_minvis(mtmp);
            if (!oldinvis && knowninvisible(mtmp)) {
                message_monster_force_visible(MSG_MONSTER_TURNS_INVISIBLE, mtmp);
                makeknown(otyp);
            }
            break;
        }
        case WAN_NOTHING:
        case WAN_LOCKING:
        case SPE_WIZARD_LOCK:
            wake = false;
            break;
        case WAN_PROBING:
            wake = false;
            reveal_invis = true;
            probe_monster(mtmp);
            makeknown(otyp);
            break;
        case WAN_OPENING:
        case SPE_KNOCK:
            wake = false; /* don't want immediate counterattack */
            if (u.uswallow && mtmp == u.ustuck) {
                if (is_animal(mtmp->data)) {
                    message_monster(MSG_ENGULFER_OPENS_ITS_MOUTH, mtmp);
                }
                expels(mtmp, mtmp->data, true);
            } else if (!!(obj = which_armor(mtmp, W_SADDLE))) {
                mtmp->misc_worn_check &= ~obj->owornmask;
                update_mon_intrinsics(mtmp, obj, false, false);
                obj->owornmask = 0L;
                obj_extract_self(obj);
                place_object(obj, mtmp->mx, mtmp->my);
                /* call stackobj() if we ever drop anything that can merge */
                newsym(mtmp->mx, mtmp->my);
            }
            break;
        case SPE_HEALING:
        case SPE_EXTRA_HEALING:
            reveal_invis = true;
            if (mtmp->data != &mons[PM_PESTILENCE]) {
                wake = false; /* wakeup() makes the target angry */
                mtmp->mhp += d(6, otyp == SPE_EXTRA_HEALING ? 8 : 4);
                if (mtmp->mhp > mtmp->mhpmax)
                    mtmp->mhp = mtmp->mhpmax;
                if (mtmp->mblinded) {
                    mtmp->mblinded = 0;
                    mtmp->mcansee = 1;
                }
                if (canseemon(mtmp)) {
                    if (disguised_mimic) {
                        if (mtmp->m_ap_type == M_AP_OBJECT && mtmp->mappearance == STRANGE_OBJECT) {
                            /* it can do better now */
                            set_mimic_sym(mtmp);
                            newsym(mtmp->mx, mtmp->my);
                        } else
                            mimic_hit_msg(mtmp, otyp);
                    } else {
                        if (otyp == SPE_EXTRA_HEALING)
                            message_monster(MSG_MONSTER_LOOKS_BETTER, mtmp);
                        else
                            message_monster(MSG_MONSTER_LOOKS_MUCH_BETTER, mtmp);
                    }
                }
                if (mtmp->mtame || mtmp->mpeaceful) {
                    adjalign(Role_if(PM_HEALER) ? 1 : sgn(u.ualign.type));
                }
            } else { /* Pestilence */
                /* Pestilence will always resist; damage is half of 3d{4,8} */
                (void)resist(mtmp, otmp->oclass, d(3, otyp == SPE_EXTRA_HEALING ? 8 : 4), TELL);
            }
            break;
        case WAN_LIGHT: /* (broken wand) */
            if (flash_hits_mon(mtmp, otmp)) {
                makeknown(WAN_LIGHT);
                reveal_invis = true;
            }
            break;
        case WAN_SLEEP: /* (broken wand) */
            /* [wakeup() doesn't rouse victims of temporary sleep,
             so it's okay to leave `wake' set to true here] */
            reveal_invis = true;
            if (sleep_monst(mtmp, d(1 + otmp->spe, 12), WAND_CLASS))
                slept_monst(mtmp);
            if (!Blind)
                makeknown(WAN_SLEEP);
            break;
        case SPE_STONE_TO_FLESH:
            if (monsndx(mtmp->data) == PM_STONE_GOLEM) {
                /* turn into flesh golem */
                if (newcham(mtmp, &mons[PM_FLESH_GOLEM], false, false)) {
                    if (canseemon(mtmp))
                        message_monster(MSG_GOLEM_TURNS_TO_FLESH, mtmp);
                } else {
                    if (canseemon(mtmp))
                        message_monster(MSG_GOLEM_LOOKS_FLESHY, mtmp);
                }
            } else
                wake = false;
            break;
        case SPE_DRAIN_LIFE:
            dmg = rnd(8);
            if (dbldam)
                dmg *= 2;
            if (otyp == SPE_DRAIN_LIFE)
                dmg += spell_damage_bonus();
            if (resists_drli(mtmp))
                shieldeff(mtmp->mx, mtmp->my);
            else if (!resist(mtmp, otmp->oclass, dmg, NOTELL) && mtmp->mhp > 0) {
                mtmp->mhp -= dmg;
                mtmp->mhpmax -= dmg;
                if (mtmp->mhp <= 0 || mtmp->mhpmax <= 0 || mtmp->m_lev < 1)
                    xkilled(mtmp, 1);
                else {
                    mtmp->m_lev--;
                    if (canseemon(mtmp))
                        message_monster(MSG_MONSTER_LOOKS_WEAKER, mtmp);
                }
            }
            break;
        default:
            impossible("What an interesting effect (%d)", otyp);
            break;
    }
    if (wake) {
        if (mtmp->mhp > 0) {
            wakeup(mtmp);
            m_respond(mtmp);
            if (mtmp->isshk && !*u.ushops)
                hot_pursuit(mtmp);
        } else if (mtmp->m_ap_type)
            seemimic(mtmp); /* might unblock if mimicing a boulder/door */
    }
    /* note: bhitpos won't be set if swallowed, but that's okay since
     * reveal_invis will be false.  We can't use mtmp->mx, my since it
     * might be an invisible worm hit on the tail.
     */
    if (reveal_invis) {
        if (mtmp->mhp > 0 && cansee(bhitpos.x, bhitpos.y) && !canspotmon(mtmp))
            map_invisible(bhitpos.x, bhitpos.y);
    }
    return 0;
}

void probe_monster (struct monst *mtmp) {
    struct obj *otmp;

    mstatusline(mtmp);
    if (notonhead) return;  /* don't show minvent for long worm tail */

    if (mtmp->minvent || mtmp->mgold) {
        for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj)
            otmp->dknown = 1;       /* treat as "seen" */
        (void) display_minventory(mtmp, MINV_ALL, (char *)0);
    } else {
        message_monster_force_visible(MSG_MONSTER_IS_NOT_CARRYING_ANYTHING, mtmp);
    }
}


/*
 * Return the object's physical location.  This only makes sense for
 * objects that are currently on the level (i.e. migrating objects
 * are nowhere).  By default, only things that can be seen (in hero's
 * inventory, monster's inventory, or on the ground) are reported.
 * By adding BURIED_TOO and/or CONTAINED_TOO flags, you can also get
 * the location of buried and contained objects.  Note that if an
 * object is carried by a monster, its reported position may change
 * from turn to turn.  This function returns false if the position
 * is not available or subject to the constraints above.
 */
bool get_obj_location(struct obj *obj, signed char *xp, signed char *yp, int locflags) {
    switch (obj->where) {
        case OBJ_INVENT:
            *xp = u.ux;
            *yp = u.uy;
            return true;
        case OBJ_FLOOR:
            *xp = obj->ox;
            *yp = obj->oy;
            return true;
        case OBJ_MINVENT:
            if (obj->ocarry->mx) {
                *xp = obj->ocarry->mx;
                *yp = obj->ocarry->my;
                return true;
            }
            break; /* !mx => migrating monster */
        case OBJ_BURIED:
            if (locflags & BURIED_TOO) {
                *xp = obj->ox;
                *yp = obj->oy;
                return true;
            }
            break;
        case OBJ_CONTAINED:
            if (locflags & CONTAINED_TOO)
                return get_obj_location(obj->ocontainer, xp, yp, locflags);
            break;
    }
    *xp = *yp = 0;
    return false;
}

/* locflags: non-zero means get location even if monster is buried */
bool get_mon_location(struct monst *mon, signed char *xp, signed char *yp, int locflags) {
    if (mon == &youmonst) {
        *xp = u.ux;
        *yp = u.uy;
        return true;
    } else if (mon->mx > 0 && (!mon->mburied || locflags)) {
        *xp = mon->mx;
        *yp = mon->my;
        return true;
    } else { /* migrating or buried */
        *xp = *yp = 0;
        return false;
    }
}

/* used by revive() and animate_statue() */
struct monst * montraits(struct obj *obj, coord *cc) {
    struct monst *mtmp = (struct monst *)0;
    struct monst *mtmp2 = (struct monst *)0;

    if (obj->oxlth && (obj->oattached == OATTACHED_MONST))
        mtmp2 = get_mtraits(obj, true);
    if (mtmp2) {
        /* save_mtraits() validated mtmp2->mnum */
        mtmp2->data = &mons[mtmp2->mnum];
        if (mtmp2->mhpmax <= 0 && !is_rider(mtmp2->data))
            return (struct monst *)0;
        mtmp = makemon(mtmp2->data, cc->x, cc->y, NO_MINVENT | MM_NOWAIT | MM_NOCOUNTBIRTH);
        if (!mtmp)
            return mtmp;

        /* heal the monster */
        if (mtmp->mhpmax > mtmp2->mhpmax && is_rider(mtmp2->data))
            mtmp2->mhpmax = mtmp->mhpmax;
        mtmp2->mhp = mtmp2->mhpmax;
        /* Get these ones from mtmp */
        mtmp2->minvent = mtmp->minvent; /*redundant*/
        /* monster ID is available if the monster died in the current
         game, but should be zero if the corpse was in a bones level
         (we cleared it when loading bones) */
        if (!mtmp2->m_id)
            mtmp2->m_id = mtmp->m_id;
        mtmp2->mx = mtmp->mx;
        mtmp2->my = mtmp->my;
        mtmp2->mux = mtmp->mux;
        mtmp2->muy = mtmp->muy;
        mtmp2->mw = mtmp->mw;
        mtmp2->wormno = mtmp->wormno;
        mtmp2->misc_worn_check = mtmp->misc_worn_check;
        mtmp2->weapon_check = mtmp->weapon_check;
        mtmp2->mtrapseen = mtmp->mtrapseen;
        mtmp2->mflee = mtmp->mflee;
        mtmp2->mburied = mtmp->mburied;
        mtmp2->mundetected = mtmp->mundetected;
        mtmp2->mfleetim = mtmp->mfleetim;
        mtmp2->mlstmv = mtmp->mlstmv;
        mtmp2->m_ap_type = mtmp->m_ap_type;
        /* set these ones explicitly */
        mtmp2->mavenge = 0;
        mtmp2->meating = 0;
        mtmp2->mleashed = 0;
        mtmp2->mtrapped = 0;
        mtmp2->msleeping = 0;
        mtmp2->mfrozen = 0;
        mtmp2->mcanmove = 1;
        /* most cancelled monsters return to normal,
         but some need to stay cancelled */
        if (!dmgtype(mtmp2->data, AD_SEDU) && !dmgtype(mtmp2->data, AD_SSEX))
            mtmp2->mcan = 0;
        mtmp2->mcansee = 1; /* set like in makemon */
        mtmp2->mblinded = 0;
        mtmp2->mstun = 0;
        mtmp2->mconf = 0;
        replmon(mtmp, mtmp2);
    }
    return mtmp2;
}

/*
 * get_container_location() returns the following information
 * about the outermost container:
 * loc argument gets set to:
 *      OBJ_INVENT      if in hero's inventory; return 0.
 *      OBJ_FLOOR       if on the floor; return 0.
 *      OBJ_BURIED      if buried; return 0.
 *      OBJ_MINVENT     if in monster's inventory; return monster.
 * container_nesting is updated with the nesting depth of the containers
 * if applicable.
 */
struct monst * get_container_location(struct obj *obj, int *loc, int *container_nesting) {
    if (!obj || !loc)
        return 0;

    if (container_nesting)
        *container_nesting = 0;
    while (obj && obj->where == OBJ_CONTAINED) {
        if (container_nesting)
            *container_nesting += 1;
        obj = obj->ocontainer;
    }
    if (obj) {
        *loc = obj->where; /* outermost container's location */
        if (obj->where == OBJ_MINVENT)
            return obj->ocarry;
    }
    return (struct monst *)0;
}

/*
 * Attempt to revive the given corpse, return the revived monster if
 * successful.  Note: this does NOT use up the corpse if it fails.
 */
struct monst * revive(struct obj *obj) {
    struct monst *mtmp = NULL;
    struct obj *container = NULL;
    int container_nesting = 0;
    signed char savetame = 0;
    bool recorporealization = false;
    bool in_container = false;
    if (obj->otyp == CORPSE) {
        int montype = obj->corpsenm;
        signed char x, y;

        if (obj->where == OBJ_CONTAINED) {
            /* deal with corpses in [possibly nested] containers */
            struct monst *carrier;
            int holder = 0;

            container = obj->ocontainer;
            carrier = get_container_location(container, &holder, &container_nesting);
            switch (holder) {
                case OBJ_MINVENT:
                    x = carrier->mx;
                    y = carrier->my;
                    in_container = true;
                    break;
                case OBJ_INVENT:
                    x = u.ux;
                    y = u.uy;
                    in_container = true;
                    break;
                case OBJ_FLOOR:
                    if (!get_obj_location(obj, &x, &y, CONTAINED_TOO))
                        return (struct monst *)0;
                    in_container = true;
                    break;
                default:
                    return (struct monst *)0;
            }
        } else {
            /* only for invent, minvent, or floor */
            if (!get_obj_location(obj, &x, &y, 0))
                return (struct monst *)0;
        }
        if (in_container) {
            /* Rules for revival from containers:
             - the container cannot be locked
             - the container cannot be heavily nested (>2 is arbitrary)
             - the container cannot be a statue or bag of holding
             (except in very rare cases for the latter)
             */
            if (!x || !y || container->olocked || container_nesting > 2 || container->otyp == STATUE || (container->otyp == BAG_OF_HOLDING && rn2(40)))
                return (struct monst *)0;
        }

        if (MON_AT(x, y)) {
            coord new_xy;

            if (enexto(&new_xy, x, y, &mons[montype]))
                x = new_xy.x, y = new_xy.y;
        }

        if (cant_create(&montype, true)) {
            /* make a zombie or worm instead */
            mtmp = makemon(&mons[montype], x, y,
            NO_MINVENT | MM_NOWAIT);
            if (mtmp) {
                mtmp->mhp = mtmp->mhpmax = 100;
                mon_adjust_speed(mtmp, 2, (struct obj *)0); /* MFAST */
            }
        } else {
            if (obj->oxlth && (obj->oattached == OATTACHED_MONST)) {
                coord xy;
                xy.x = x;
                xy.y = y;
                mtmp = montraits(obj, &xy);
                if (mtmp && mtmp->mtame && !mtmp->isminion)
                    wary_dog(mtmp, true);
            } else
                mtmp = makemon(&mons[montype], x, y,
                NO_MINVENT | MM_NOWAIT | MM_NOCOUNTBIRTH);
            if (mtmp) {
                if (obj->oxlth && (obj->oattached == OATTACHED_M_ID)) {
                    unsigned m_id;
                    struct monst *ghost;
                    (void)memcpy((void *)&m_id, (void *)obj->oextra, sizeof(m_id));
                    ghost = find_mid(m_id, FM_FMON);
                    if (ghost && ghost->data == &mons[PM_GHOST]) {
                        int x2, y2;
                        x2 = ghost->mx;
                        y2 = ghost->my;
                        if (ghost->mtame)
                            savetame = ghost->mtame;
                        if (canseemon(ghost))
                            message_monster(MSG_DRAWN_INTO_FORMER_BODY, ghost);
                        mondead(ghost);
                        recorporealization = true;
                        newsym(x2, y2);
                    }
                    /* don't mess with obj->oxlth here */
                    obj->oattached = OATTACHED_NOTHING;
                }
                /* Monster retains its name */
                if (obj->onamelth)
                    mtmp = christen_monst(mtmp, ONAME(obj));
                /* flag the quest leader as alive. */
                if (mtmp->data->msound == MS_LEADER || mtmp->m_id == quest_status.leader_m_id)
                    quest_status.leader_is_dead = false;
            }
        }
        if (mtmp) {
            if (obj->oeaten)
                mtmp->mhp = eaten_stat(mtmp->mhp, obj);
            /* track that this monster was revived at least once */
            mtmp->mrevived = 1;

            if (recorporealization) {
                /* If mtmp is revivification of former tame ghost*/
                if (savetame) {
                    struct monst *mtmp2 = tamedog(mtmp, (struct obj *)0);
                    if (mtmp2) {
                        mtmp2->mtame = savetame;
                        mtmp = mtmp2;
                    }
                }
                /* was ghost, now alive, it's all very confusing */
                mtmp->mconf = 1;
            }

            switch (obj->where) {
                case OBJ_INVENT:
                    useup(obj);
                    break;
                case OBJ_FLOOR:
                    /* in case MON_AT+enexto for invisible mon */
                    x = obj->ox, y = obj->oy;
                    /* not useupf(), which charges */
                    if (obj->quan > 1L)
                        obj = splitobj(obj, 1L);
                    delobj(obj);
                    newsym(x, y);
                    break;
                case OBJ_MINVENT:
                    m_useup(obj->ocarry, obj);
                    break;
                case OBJ_CONTAINED:
                    obj_extract_self(obj);
                    obfree(obj, (struct obj *)0);
                    break;
                default:
                    panic("revive");
            }
        }
    }
    return mtmp;
}

static void revive_egg(struct obj *obj) {
    /*
     * Note: generic eggs with corpsenm set to NON_PM will never hatch.
     */
    if (obj->otyp != EGG)
        return;
    if (obj->corpsenm != NON_PM && !dead_species(obj->corpsenm, true))
        attach_egg_hatch_timeout(obj);
}

/* try to revive all corpses and eggs carried by `mon' */
int unturn_dead (struct monst *mon) {
    char corpse[BUFSZ];
    int once = 0, res = 0;

    bool youseeit = (mon == &youmonst) ? true : canseemon(mon);
    struct obj * otmp2 = (mon == &youmonst) ? invent : mon->minvent;

    char owner[BUFSZ];
    if (mon == &youmonst) {
        strcpy(owner, "Your");
    } else {
        char name[BUFSZ];
        Monnam(name, BUFSZ, mon);
        nh_slprintf(owner, BUFSZ, "%s%s", name, possessive_suffix(name));
    }

    struct obj *otmp;
    while ((otmp = otmp2) != 0) {
        otmp2 = otmp->nobj;
        if (otmp->otyp == EGG)
            revive_egg(otmp);
        if (otmp->otyp != CORPSE) continue;
        /* save the name; the object is liable to go away */
        if (youseeit) strcpy(corpse, corpse_xname(otmp, true));

        /* for a merged group, only one is revived; should this be fixed? */
        struct monst *mtmp2;
        if ((mtmp2 = revive(otmp)) != 0) {
            ++res;
            if (youseeit) {
                pline("%s %s suddenly comes alive!", owner, corpse);
            } else if (canseemon(mtmp2)) {
                message_monster(MSG_A_MONSTER_SUDDENLY_APPEARS, mtmp2);
            }
        }
    }
    return res;
}

static const char charged_objs[] = { WAND_CLASS, WEAPON_CLASS, ARMOR_CLASS, 0 };

static void
costly_cancel (struct obj *obj)
{
        char objroom;
        struct monst *shkp = (struct monst *)0;

        if (obj->no_charge) return;

        switch (obj->where) {
        case OBJ_INVENT:
                if (obj->unpaid) {
                    shkp = shop_keeper(*u.ushops);
                    if (!shkp) return;
                    Norep("You cancel an unpaid object, you pay for it!");
                    bill_dummy_object(obj);
                }
                break;
        case OBJ_FLOOR:
                objroom = *in_rooms(obj->ox, obj->oy, SHOPBASE);
                shkp = shop_keeper(objroom);
                if (!shkp || !inhishop(shkp)) return;
                if (costly_spot(u.ux, u.uy) && objroom == *u.ushops) {
                    Norep("You cancel it, you pay for it!");
                    bill_dummy_object(obj);
                } else
                    (void) stolen_value(obj, obj->ox, obj->oy, false, false);
                break;
        }
}

/* cancel obj, possibly carried by you or a monster */
void cancel_item(struct obj *obj) {
    bool u_ring = (obj == uleft) || (obj == uright);
    bool holy = (obj->otyp == POT_WATER && obj->blessed);

    switch (obj->otyp) {
        case RIN_GAIN_STRENGTH:
            if ((obj->owornmask & W_RING) && u_ring) {
                ABON(A_STR) -= obj->spe;
                flags.botl = 1;
            }
            break;
        case RIN_GAIN_CONSTITUTION:
            if ((obj->owornmask & W_RING) && u_ring) {
                ABON(A_CON) -= obj->spe;
                flags.botl = 1;
            }
            break;
        case RIN_ADORNMENT:
            if ((obj->owornmask & W_RING) && u_ring) {
                ABON(A_CHA) -= obj->spe;
                flags.botl = 1;
            }
            break;
        case RIN_INCREASE_ACCURACY:
            if ((obj->owornmask & W_RING) && u_ring)
                u.uhitinc -= obj->spe;
            break;
        case RIN_INCREASE_DAMAGE:
            if ((obj->owornmask & W_RING) && u_ring)
                u.udaminc -= obj->spe;
            break;
        case GAUNTLETS_OF_DEXTERITY:
            if ((obj->owornmask & W_ARMG) && (obj == uarmg)) {
                ABON(A_DEX) -= obj->spe;
                flags.botl = 1;
            }
            break;
        case HELM_OF_BRILLIANCE:
            if ((obj->owornmask & W_ARMH) && (obj == uarmh)) {
                ABON(A_INT) -= obj->spe;
                ABON(A_WIS) -= obj->spe;
                flags.botl = 1;
            }
            break;
            /* case RIN_PROTECTION:  not needed */
    }
    if (objects[obj->otyp].oc_magic || (obj->spe && (obj->oclass == ARMOR_CLASS || obj->oclass == WEAPON_CLASS || is_weptool(obj))) || obj->otyp == POT_ACID || obj->otyp == POT_SICKNESS) {
        if (obj->spe != ((obj->oclass == WAND_CLASS) ? -1 : 0) && obj->otyp != WAN_CANCELLATION && obj->otyp != MAGIC_LAMP && obj->otyp != CANDELABRUM_OF_INVOCATION) {
            costly_cancel(obj);
            obj->spe = (obj->oclass == WAND_CLASS) ? -1 : 0;
        }
        switch (obj->oclass) {
            case SCROLL_CLASS:
                costly_cancel(obj);
                obj->otyp = SCR_BLANK_PAPER;
                obj->spe = 0;
                break;
            case SPBOOK_CLASS:
                if (obj->otyp != SPE_CANCELLATION && obj->otyp != SPE_BOOK_OF_THE_DEAD) {
                    costly_cancel(obj);
                    obj->otyp = SPE_BLANK_PAPER;
                }
                break;
            case POTION_CLASS:
                costly_cancel(obj);
                if (obj->otyp == POT_SICKNESS || obj->otyp == POT_SEE_INVISIBLE) {
                    /* sickness is "biologically contaminated" fruit juice; cancel it
                     * and it just becomes fruit juice... whereas see invisible
                     * tastes like "enchanted" fruit juice, it similarly cancels.
                     */
                    obj->otyp = POT_FRUIT_JUICE;
                } else {
                    obj->otyp = POT_WATER;
                    obj->odiluted = 0; /* same as any other water */
                }
                break;
        }
    }
    if (holy)
        costly_cancel(obj);
    unbless(obj);
    uncurse(obj);
    return;
}

/* Remove a positive enchantment or charge from obj,
 * possibly carried by you or a monster
 */
bool drain_item(struct obj *obj) {
    bool u_ring;

    /* Is this a charged/enchanted object? */
    if (!obj || (!objects[obj->otyp].oc_charged && obj->oclass != WEAPON_CLASS && obj->oclass != ARMOR_CLASS && !is_weptool(obj)) || obj->spe <= 0)
        return (false);
    if (obj_resists(obj, 10, 90))
        return (false);

    /* Charge for the cost of the object */
    costly_cancel(obj); /* The term "cancel" is okay for now */

    /* Drain the object and any implied effects */
    obj->spe--;
    u_ring = (obj == uleft) || (obj == uright);
    switch (obj->otyp) {
        case RIN_GAIN_STRENGTH:
            if ((obj->owornmask & W_RING) && u_ring) {
                ABON(A_STR)--;
                flags.botl = 1;
            }
            break;
        case RIN_GAIN_CONSTITUTION:
            if ((obj->owornmask & W_RING) && u_ring) {
                ABON(A_CON)--;
                flags.botl = 1;
            }
            break;
        case RIN_ADORNMENT:
            if ((obj->owornmask & W_RING) && u_ring) {
                ABON(A_CHA)--;
                flags.botl = 1;
            }
            break;
        case RIN_INCREASE_ACCURACY:
            if ((obj->owornmask & W_RING) && u_ring)
                u.uhitinc--;
            break;
        case RIN_INCREASE_DAMAGE:
            if ((obj->owornmask & W_RING) && u_ring)
                u.udaminc--;
            break;
        case HELM_OF_BRILLIANCE:
            if ((obj->owornmask & W_ARMH) && (obj == uarmh)) {
                ABON(A_INT)--;
                ABON(A_WIS)--;
                flags.botl = 1;
            }
            break;
        case GAUNTLETS_OF_DEXTERITY:
            if ((obj->owornmask & W_ARMG) && (obj == uarmg)) {
                ABON(A_DEX)--;
                flags.botl = 1;
            }
            break;
        case RIN_PROTECTION:
            flags.botl = 1;
            break;
    }
    if (carried(obj))
        update_inventory();
    return (true);
}


/* percent chance for ordinary objects, artifacts */
bool obj_resists(struct obj *obj, int ochance, int achance) {
    if (obj->otyp == AMULET_OF_YENDOR || obj->otyp == SPE_BOOK_OF_THE_DEAD || obj->otyp == CANDELABRUM_OF_INVOCATION || obj->otyp == BELL_OF_OPENING || (obj->otyp == CORPSE && is_rider(&mons[obj->corpsenm]))) {
        return true;
    } else {
        int chance = rn2(100);
        return ((bool)(chance < (obj->oartifact ? achance : ochance)));
    }
}

bool obj_shudders(struct obj *obj) {
    int zap_odds;

    if (obj->oclass == WAND_CLASS)
        zap_odds = 3; /* half-life = 2 zaps */
    else if (obj->cursed)
        zap_odds = 3; /* half-life = 2 zaps */
    else if (obj->blessed)
        zap_odds = 12; /* half-life = 8 zaps */
    else
        zap_odds = 8; /* half-life = 6 zaps */

    /* adjust for "large" quantities of identical things */
    if (obj->quan > 4L)
        zap_odds /= 2;

    return ((bool)(!rn2(zap_odds)));
}

/* Use up at least minwt number of things made of material mat.
 * There's also a chance that other stuff will be used up.  Finally,
 * there's a random factor here to keep from always using the stuff
 * at the top of the pile.
 */
static void polyuse(struct obj *objhdr, int mat, int minwt) {
    struct obj *otmp, *otmp2;

    for (otmp = objhdr; minwt > 0 && otmp; otmp = otmp2) {
        otmp2 = otmp->nexthere;
        if (otmp == uball || otmp == uchain)
            continue;
        if (obj_resists(otmp, 0, 0))
            continue; /* preserve unique objects */
        if (otmp->otyp == SCR_MAIL)
            continue;

        if (((int)objects[otmp->otyp].oc_material == mat) == (rn2(minwt + 1) != 0)) {
            /* appropriately add damage to bill */
            if (costly_spot(otmp->ox, otmp->oy)) {
                if (*u.ushops)
                    addtobill(otmp, false, false, false);
                else
                    (void)stolen_value(otmp, otmp->ox, otmp->oy, false, false);
            }
            if (otmp->quan < LARGEST_INT)
                minwt -= (int)otmp->quan;
            else
                minwt = 0;
            delobj(otmp);
        }
    }
}

/*
 * Polymorph some of the stuff in this pile into a monster, preferably
 * a golem of the kind okind.
 */
static void create_polymon(struct obj *obj, int okind) {
    struct permonst *mdat = (struct permonst *)0;
    struct monst *mtmp;
    const char *material;
    int pm_index;

    /* no golems if you zap only one object -- not enough stuff */
    if (!obj || (!obj->nexthere && obj->quan == 1L))
        return;

    /* some of these choices are arbitrary */
    switch (okind) {
        case IRON:
        case METAL:
        case MITHRIL:
            pm_index = PM_IRON_GOLEM;
            material = "metal ";
            break;
        case COPPER:
        case SILVER:
        case PLATINUM:
        case GEMSTONE:
        case MINERAL:
            pm_index = rn2(2) ? PM_STONE_GOLEM : PM_CLAY_GOLEM;
            material = "lithic ";
            break;
        case 0:
        case FLESH:
            /* there is no flesh type, but all food is type 0, so we use it */
            pm_index = PM_FLESH_GOLEM;
            material = "organic ";
            break;
        case WOOD:
            pm_index = PM_WOOD_GOLEM;
            material = "wood ";
            break;
        case LEATHER:
            pm_index = PM_LEATHER_GOLEM;
            material = "leather ";
            break;
        case CLOTH:
            pm_index = PM_ROPE_GOLEM;
            material = "cloth ";
            break;
        case BONE:
            pm_index = PM_SKELETON; /* nearest thing to "bone golem" */
            material = "bony ";
            break;
        case GOLD:
            pm_index = PM_GOLD_GOLEM;
            material = "gold ";
            break;
        case GLASS:
            pm_index = PM_GLASS_GOLEM;
            material = "glassy ";
            break;
        case PAPER:
            pm_index = PM_PAPER_GOLEM;
            material = "paper ";
            break;
        default:
            /* if all else fails... */
            pm_index = PM_STRAW_GOLEM;
            material = "";
            break;
    }

    if (!(mvitals[pm_index].mvflags & G_GENOD))
        mdat = &mons[pm_index];

    mtmp = makemon(mdat, obj->ox, obj->oy, NO_MM_FLAGS);
    polyuse(obj, okind, (int)mons[pm_index].cwt);

    if (mtmp && cansee(mtmp->mx, mtmp->my)) {
        char name[BUFSZ];
        a_monnam(name, BUFSZ, mtmp);
        pline("Some %sobjects meld, and %s arises from the pile!", material, name);
    }
}

/* Assumes obj is on the floor. */
void do_osshock(struct obj *obj) {
    long i;

    if (obj->otyp == SCR_MAIL)
        return;
    obj_zapped = true;

    if (poly_zapped < 0) {
        /* some may metamorphosize */
        for (i = obj->quan; i; i--) {
            if (!rn2(Luck + 45)) {
                poly_zapped = objects[obj->otyp].oc_material;
                break;
            }
        }
    }

    /* if quan > 1 then some will survive intact */
    if (obj->quan > 1L) {
        if (obj->quan > LARGEST_INT)
            obj = splitobj(obj, (long)rnd(30000));
        else
            obj = splitobj(obj, (long)rnd((int)obj->quan - 1));
    }

    /* appropriately add damage to bill */
    if (costly_spot(obj->ox, obj->oy)) {
        if (*u.ushops)
            addtobill(obj, false, false, false);
        else
            (void)stolen_value(obj, obj->ox, obj->oy, false, false);
    }

    /* zap the object */
    delobj(obj);
}

static bool usesCorpseEnm(int typ) {
    return typ == CORPSE || typ == STATUE || typ == FIGURINE;
}

/*
 * Polymorph the object to the given object ID.  If the ID is STRANGE_OBJECT
 * then pick random object from the source's class (this is the standard
 * "polymorph" case).  If ID is set to a specific object, inhibit fusing
 * n objects into 1.  This could have been added as a flag, but currently
 * it is tied to not being the standard polymorph case. The new polymorphed
 * object replaces obj in its link chains.  Return value is a pointer to
 * the new object.
 *
 * This should be safe to call for an object anywhere.
 */
struct obj * poly_obj(struct obj *obj, int id) {
    struct obj *otmp;
    signed char ox, oy;
    bool can_merge = (id == STRANGE_OBJECT);
    int obj_location = obj->where;

    if (obj->otyp == BOULDER && In_sokoban(&u.uz))
        change_luck(-1); /* Sokoban guilt */
    if (id == STRANGE_OBJECT) { /* preserve symbol */
        int try_limit = 3;
        /* Try up to 3 times to make the magic-or-not status of
         the new item be the same as it was for the old one. */
        otmp = (struct obj *)0;
        do {
            if (otmp)
                delobj(otmp);
            otmp = mkobj(obj->oclass, false);
        } while (--try_limit > 0 && objects[obj->otyp].oc_magic != objects[otmp->otyp].oc_magic);
    } else {
        /* literally replace obj with this new thing */
        otmp = mksobj(id, false, false);
        /* Actually more things use corpsenm but they polymorph differently */
        if (usesCorpseEnm(obj->otyp) && usesCorpseEnm(id))
            otmp->corpsenm = obj->corpsenm;
    }

    /* preserve quantity */
    otmp->quan = obj->quan;
    /* preserve the shopkeepers (lack of) interest */
    otmp->no_charge = obj->no_charge;
    /* preserve inventory letter if in inventory */
    if (obj_location == OBJ_INVENT)
        otmp->invlet = obj->invlet;
    /* You can't send yourself 100 mail messages and then
     * polymorph them into useful scrolls
     */
    if (obj->otyp == SCR_MAIL) {
        otmp->otyp = SCR_MAIL;
        otmp->spe = 1;
    }

    /* avoid abusing eggs laid by you */
    if (obj->otyp == EGG && obj->spe) {
        int mnum, tryct = 100;

        /* first, turn into a generic egg */
        if (otmp->otyp == EGG)
            kill_egg(otmp);
        else {
            otmp->otyp = EGG;
            otmp->owt = weight(otmp);
        }
        otmp->corpsenm = NON_PM;
        otmp->spe = 0;

        /* now change it into something layed by the hero */
        while (tryct--) {
            mnum = can_be_hatched(random_monster());
            if (mnum != NON_PM && !dead_species(mnum, true)) {
                otmp->spe = 1; /* layed by hero */
                otmp->corpsenm = mnum;
                attach_egg_hatch_timeout(otmp);
                break;
            }
        }
    }

    /* keep special fields (including charges on wands) */
    if (index(charged_objs, otmp->oclass))
        otmp->spe = obj->spe;
    otmp->recharged = obj->recharged;

    otmp->cursed = obj->cursed;
    otmp->blessed = obj->blessed;
    otmp->oeroded = obj->oeroded;
    otmp->oeroded2 = obj->oeroded2;
    if (!is_flammable(otmp) && !is_rustprone(otmp))
        otmp->oeroded = 0;
    if (!is_corrodeable(otmp) && !is_rottable(otmp))
        otmp->oeroded2 = 0;
    if (is_damageable(otmp))
        otmp->oerodeproof = obj->oerodeproof;

    /* Keep chest/box traps and poisoned ammo if we may */
    if (obj->otrapped && Is_box(otmp))
        otmp->otrapped = true;

    if (obj->opoisoned && is_poisonable(otmp))
        otmp->opoisoned = true;

    if (id == STRANGE_OBJECT && obj->otyp == CORPSE) {
        /* turn crocodile corpses into shoes */
        if (obj->corpsenm == PM_CROCODILE) {
            otmp->otyp = LOW_BOOTS;
            otmp->oclass = ARMOR_CLASS;
            otmp->spe = 0;
            otmp->oeroded = 0;
            otmp->oerodeproof = true;
            otmp->quan = 1L;
            otmp->cursed = false;
        }
    }

    /* no box contents --KAA */
    if (Has_contents(otmp))
        delete_contents(otmp);

    /* 'n' merged objects may be fused into 1 object */
    if (otmp->quan > 1L && (!objects[otmp->otyp].oc_merge || (can_merge && otmp->quan > (long)rn2(1000))))
        otmp->quan = 1L;

    switch (otmp->oclass) {

        case TOOL_CLASS:
            if (otmp->otyp == MAGIC_LAMP) {
                otmp->otyp = OIL_LAMP;
                otmp->age = 1500L; /* "best" oil lamp possible */
            } else if (otmp->otyp == MAGIC_MARKER) {
                otmp->recharged = 1; /* degraded quality */
            }
            /* don't care about the recharge count of other tools */
            break;

        case WAND_CLASS:
            while (otmp->otyp == WAN_WISHING || otmp->otyp == WAN_POLYMORPH)
                otmp->otyp = rnd_class(WAN_LIGHT, WAN_LIGHTNING);
            /* altering the object tends to degrade its quality
             (analogous to spellbook `read count' handling) */
            if ((int)otmp->recharged < rn2(7)) /* recharge_limit */
                otmp->recharged++;
            break;

        case POTION_CLASS:
            while (otmp->otyp == POT_POLYMORPH)
                otmp->otyp = rnd_class(POT_GAIN_ABILITY, POT_WATER);
            break;

        case SPBOOK_CLASS:
            while (otmp->otyp == SPE_POLYMORPH)
                otmp->otyp = rnd_class(SPE_DIG, SPE_BLANK_PAPER);
            /* reduce spellbook abuse */
            otmp->spestudied = obj->spestudied + 1;
            break;

        case GEM_CLASS:
            if (otmp->quan > (long)rnd(4) && objects[obj->otyp].oc_material == MINERAL && objects[otmp->otyp].oc_material != MINERAL) {
                otmp->otyp = ROCK; /* transmutation backfired */
                otmp->quan /= 2L; /* some material has been lost */
            }
            break;
    }

    /* update the weight */
    otmp->owt = weight(otmp);

    /* for now, take off worn items being polymorphed */
    if (obj_location == OBJ_INVENT) {
        if (id == STRANGE_OBJECT)
            remove_worn_item(obj, true);
        else {
            /* This is called only for stone to flesh.  It's a lot simpler
             * than it otherwise might be.  We don't need to check for
             * special effects when putting them on (no meat objects have
             * any) and only three worn masks are possible.
             */
            otmp->owornmask = obj->owornmask;
            remove_worn_item(obj, true);
            setworn(otmp, otmp->owornmask);
            if (otmp->owornmask & LEFT_RING)
                uleft = otmp;
            if (otmp->owornmask & RIGHT_RING)
                uright = otmp;
            if (otmp->owornmask & W_WEP)
                uwep = otmp;
            if (otmp->owornmask & W_SWAPWEP)
                uswapwep = otmp;
            if (otmp->owornmask & W_QUIVER)
                uquiver = otmp;
            goto no_unwear;
        }
    }

    /* preserve the mask in case being used by something else */
    otmp->owornmask = obj->owornmask;
    no_unwear:

    if (obj_location == OBJ_FLOOR && obj->otyp == BOULDER && otmp->otyp != BOULDER)
        unblock_point(obj->ox, obj->oy);

    /* ** we are now done adjusting the object ** */

    /* swap otmp for obj */
    replace_object(obj, otmp);
    if (obj_location == OBJ_INVENT) {
        /*
         * We may need to do extra adjustments for the hero if we're
         * messing with the hero's inventory.  The following calls are
         * equivalent to calling freeinv on obj and addinv on otmp,
         * while doing an in-place swap of the actual objects.
         */
        freeinv_core(obj);
        addinv_core1(otmp);
        addinv_core2(otmp);
    }

    if ((!carried(otmp) || obj->unpaid) && get_obj_location(otmp, &ox, &oy, BURIED_TOO | CONTAINED_TOO) && costly_spot(ox, oy)) {
        struct monst *shkp = shop_keeper(*in_rooms(ox, oy, SHOPBASE));

        if ((!obj->no_charge || (Has_contents(obj) && (contained_cost(obj, shkp, 0L, false, false) != 0L))) && inhishop(shkp)) {
            if (shkp->mpeaceful) {
                if (*u.ushops && *in_rooms(u.ux, u.uy, 0) == *in_rooms(shkp->mx, shkp->my, 0) && !costly_spot(u.ux, u.uy)) {
                    make_angry_shk(shkp, ox, oy);
                } else {
                    char name[BUFSZ];
                    Monnam(name, BUFSZ, shkp);
                    pline("%s gets angry!", name);
                    hot_pursuit(shkp);
                }
            } else {
                char name[BUFSZ];
                Monnam(name, BUFSZ, shkp);
                Norep("%s is furious!", name);
            }
        }
    }
    delobj(obj);
    return otmp;
}

/*
 * Object obj was hit by the effect of the wand/spell otmp.  Return
 * non-zero if the wand/spell had any effect.
 */
int bhito(struct obj *obj, struct obj *otmp) {
    int res = 1; /* affected object by default */
    signed char refresh_x, refresh_y;

    if (obj->bypass) {
        /* The bypass bit is currently only used as follows:
         *
         * POLYMORPH - When a monster being polymorphed drops something
         *             from its inventory as a result of the change.
         *             If the items fall to the floor, they are not
         *             subject to direct subsequent polymorphing
         *             themselves on that same zap. This makes it
         *             consistent with items that remain in the
         *             monster's inventory. They are not polymorphed
         *             either.
         * UNDEAD_TURNING - When an undead creature gets killed via
         *             undead turning, prevent its corpse from being
         *             immediately revived by the same effect.
         *
         * The bypass bit on all objects is reset each turn, whenever
         * flags.bypasses is set.
         *
         * We check the obj->bypass bit above AND flags.bypasses
         * as a safeguard against any stray occurrence left in an obj
         * struct someplace, although that should never happen.
         */
        if (flags.bypasses)
            return 0;
        else {
            obj->bypass = 0;
        }
    }

    /*
     * Some parts of this function expect the object to be on the floor
     * obj->{ox,oy} to be valid.  The exception to this (so far) is
     * for the STONE_TO_FLESH spell.
     */
    if (!(obj->where == OBJ_FLOOR || otmp->otyp == SPE_STONE_TO_FLESH))
        impossible("bhito: obj is not floor or Stone To Flesh spell");

    if (obj == uball) {
        res = 0;
    } else if (obj == uchain) {
        if (otmp->otyp == WAN_OPENING || otmp->otyp == SPE_KNOCK) {
            unpunish();
            makeknown(otmp->otyp);
        } else
            res = 0;
    } else {
        switch (otmp->otyp) {
            case WAN_POLYMORPH:
            case SPE_POLYMORPH:
                if (obj->otyp == WAN_POLYMORPH || obj->otyp == SPE_POLYMORPH || obj->otyp == POT_POLYMORPH || obj_resists(obj, 5, 95)) {
                    res = 0;
                    break;
                }
                /* KMH, conduct */
                u.uconduct.polypiles++;
                /* any saved lock context will be dangerously obsolete */
                if (Is_box(obj))
                    (void)boxlock(obj, otmp);

                if (obj_shudders(obj)) {
                    if (cansee(obj->ox, obj->oy))
                        makeknown(otmp->otyp);
                    do_osshock(obj);
                    break;
                }
                obj = poly_obj(obj, STRANGE_OBJECT);
                newsym(obj->ox, obj->oy);
                break;
            case WAN_PROBING:
                res = !obj->dknown;
                /* target object has now been "seen (up close)" */
                obj->dknown = 1;
                if (Is_container(obj) || obj->otyp == STATUE) {
                    if (!obj->cobj) {
                        char it_is[BUFSZ];
                        Tobjnam(it_is, BUFSZ, obj, "are");
                        pline("%s empty.", it_is);
                    } else {
                        struct obj *o;
                        /* view contents (not recursively) */
                        for (o = obj->cobj; o; o = o->nobj)
                            o->dknown = 1; /* "seen", even if blind */
                        (void)display_cinventory(obj);
                    }
                    res = 1;
                }
                if (res)
                    makeknown(WAN_PROBING);
                break;
            case WAN_STRIKING:
            case SPE_FORCE_BOLT:
                if (obj->otyp == BOULDER)
                    fracture_rock(obj);
                else if (obj->otyp == STATUE)
                    (void)break_statue(obj);
                else {
                    if (!flags.mon_moving)
                        (void)hero_breaks(obj, obj->ox, obj->oy, false);
                    else
                        (void)breaks(obj, obj->ox, obj->oy);
                    res = 0;
                }
                /* BUG[?]: shouldn't this depend upon you seeing it happen? */
                makeknown(otmp->otyp);
                break;
            case WAN_CANCELLATION:
            case SPE_CANCELLATION:
                cancel_item(obj);
                newsym(obj->ox, obj->oy); /* might change color */
                break;
            case SPE_DRAIN_LIFE:
                (void)drain_item(obj);
                break;
            case WAN_TELEPORTATION:
            case SPE_TELEPORT_AWAY:
                rloco(obj);
                break;
            case WAN_MAKE_INVISIBLE:
                break;
            case WAN_UNDEAD_TURNING:
            case SPE_TURN_UNDEAD:
                if (obj->otyp == EGG)
                    revive_egg(obj);
                else
                    res = !!revive(obj);
                break;
            case WAN_OPENING:
            case SPE_KNOCK:
            case WAN_LOCKING:
            case SPE_WIZARD_LOCK:
                if (Is_box(obj))
                    res = boxlock(obj, otmp);
                else
                    res = 0;
                if (res /* && otmp->oclass == WAND_CLASS */)
                    makeknown(otmp->otyp);
                break;
            case WAN_SLOW_MONSTER: /* no effect on objects */
            case SPE_SLOW_MONSTER:
            case WAN_SPEED_MONSTER:
            case WAN_NOTHING:
            case SPE_HEALING:
            case SPE_EXTRA_HEALING:
                res = 0;
                break;
            case SPE_STONE_TO_FLESH:
                refresh_x = obj->ox;
                refresh_y = obj->oy;
                if (objects[obj->otyp].oc_material != MINERAL && objects[obj->otyp].oc_material != GEMSTONE) {
                    res = 0;
                    break;
                }
                /* add more if stone objects are added.. */
                switch (objects[obj->otyp].oc_class) {
                    case ROCK_CLASS: /* boulders and statues */
                        if (obj->otyp == BOULDER) {
                            obj = poly_obj(obj, HUGE_CHUNK_OF_MEAT);
                            goto smell;
                        } else if (obj->otyp == STATUE) {
                            signed char oox, ooy;

                            (void)get_obj_location(obj, &oox, &ooy, 0);
                            refresh_x = oox;
                            refresh_y = ooy;
                            if (vegetarian(&mons[obj->corpsenm])) {
                                /* Don't animate monsters that aren't flesh */
                                obj = poly_obj(obj, MEATBALL);
                                goto smell;
                            }
                            if (!animate_statue(obj, oox, ooy,
                            ANIMATE_SPELL, (int *)0)) {
                                struct obj *item;
                                makecorpse: if (mons[obj->corpsenm].geno & (G_NOCORPSE | G_UNIQ)) {
                                    res = 0;
                                    break;
                                }
                                /* Unlikely to get here since genociding
                                 * monsters also sets the G_NOCORPSE flag.
                                 * Drop the contents, poly_obj looses them.
                                 */
                                while ((item = obj->cobj) != 0) {
                                    obj_extract_self(item);
                                    place_object(item, oox, ooy);
                                }
                                obj = poly_obj(obj, CORPSE);
                                break;
                            }
                        } else { /* new rock class object... */
                            /* impossible? */
                            res = 0;
                        }
                        break;
                    case TOOL_CLASS: /* figurine */
                    {
                        struct monst *mon;
                        signed char oox, ooy;

                        if (obj->otyp != FIGURINE) {
                            res = 0;
                            break;
                        }
                        if (vegetarian(&mons[obj->corpsenm])) {
                            /* Don't animate monsters that aren't flesh */
                            obj = poly_obj(obj, MEATBALL);
                            goto smell;
                        }
                        (void)get_obj_location(obj, &oox, &ooy, 0);
                        refresh_x = oox;
                        refresh_y = ooy;
                        mon = makemon(&mons[obj->corpsenm], oox, ooy, NO_MM_FLAGS);
                        if (mon) {
                            delobj(obj);
                            if (cansee(mon->mx, mon->my))
                                pline_The("figurine animates!");
                            break;
                        }
                        goto makecorpse;
                    }
                        /* maybe add weird things to become? */
                    case RING_CLASS: /* some of the rings are stone */
                        obj = poly_obj(obj, MEAT_RING);
                        goto smell;
                    case WAND_CLASS: /* marble wand */
                        obj = poly_obj(obj, MEAT_STICK);
                        goto smell;
                    case GEM_CLASS: /* rocks & gems */
                        obj = poly_obj(obj, MEATBALL);
                        smell: if (herbivorous(youmonst.data) && (!carnivorous(youmonst.data) || Role_if(PM_MONK) || !u.uconduct.unvegetarian))
                            Norep("You smell the odor of meat.");
                        else
                            Norep("You smell a delicious smell.");
                        break;
                    case WEAPON_CLASS: /* crysknife */
                        /* fall through */
                    default:
                        res = 0;
                        break;
                }
                newsym(refresh_x, refresh_y);
                break;
            default:
                impossible("What an interesting effect (%d)", otmp->otyp);
                break;
        }
    }
    return res;
}

/* returns nonzero if something was hit */
int bhitpile(struct obj *obj, int (*fhito)(struct obj *, struct obj *), int tx, int ty) {
    int hitanything = 0;
    struct obj *otmp, *next_obj;

    if (obj->otyp == SPE_FORCE_BOLT || obj->otyp == WAN_STRIKING) {
        struct trap *t = t_at(tx, ty);

        /* We can't settle for the default calling sequence of
         bhito(otmp) -> break_statue(otmp) -> activate_statue_trap(ox,oy)
         because that last call might end up operating on our `next_obj'
         (below), rather than on the current object, if it happens to
         encounter a statue which mustn't become animated. */
        if (t && t->ttyp == STATUE_TRAP && activate_statue_trap(t, tx, ty, true) && obj->otyp == WAN_STRIKING)
            makeknown(obj->otyp);
    }

    poly_zapped = -1;
    for (otmp = level.objects[tx][ty]; otmp; otmp = next_obj) {
        /* Fix for polymorph bug, Tim Wright */
        next_obj = otmp->nexthere;
        hitanything += (*fhito)(otmp, obj);
    }
    if (poly_zapped >= 0)
        create_polymon(level.objects[tx][ty], poly_zapped);

    return hitanything;
}

/*
 * zappable - returns 1 if zap is available, 0 otherwise.
 *            it removes a charge from the wand if zappable.
 * added by GAN 11/03/86
 */
int zappable(struct obj *wand) {
    if (wand->spe < 0 || (wand->spe == 0 && rn2(121)))
        return 0;
    if (wand->spe == 0)
        You("wrest one last charge from the worn-out wand.");
    wand->spe--;
    return 1;
}

/*
 * zapnodir - zaps a NODIR wand/spell.
 * added by GAN 11/03/86
 */
void zapnodir(struct obj *obj) {
    bool known = false;

    switch (obj->otyp) {
        case WAN_LIGHT:
        case SPE_LIGHT:
            litroom(true, obj);
            if (!Blind)
                known = true;
            break;
        case WAN_SECRET_DOOR_DETECTION:
        case SPE_DETECT_UNSEEN:
            if (!findit())
                return;
            if (!Blind)
                known = true;
            break;
        case WAN_CREATE_MONSTER:
            known = create_critters(rn2(23) ? 1 : rn1(7, 2), (struct permonst *)0);
            break;
        case WAN_WISHING:
            known = true;
            if (Luck + rn2(5) < 0) {
                pline("Unfortunately, nothing happens.");
                break;
            }
            makewish();
            break;
        case WAN_ENLIGHTENMENT:
            known = true;
            You_feel("self-knowledgeable...");
            enlightenment(false);
            pline_The("feeling subsides.");
            exercise(A_WIS, true);
            break;
    }
    if (known && !objects[obj->otyp].oc_name_known) {
        makeknown(obj->otyp);
        more_experienced(0, 10);
    }
}

static void backfire(struct obj *otmp) {
    otmp->in_use = true; /* in case losehp() is fatal */
    pline("%s suddenly explodes!", The(xname(otmp)));
    losehp(d(otmp->spe + 2, 6), "exploding wand", KILLED_BY_AN);
    useup(otmp);
}

static const char zap_syms[] = { WAND_CLASS, 0 };

int dozap(void) {
    struct obj *obj;
    int damage;

    if (check_capacity((char *)0))
        return (0);
    obj = getobj(zap_syms, "zap");
    if (!obj)
        return (0);

    check_unpaid(obj);

    /* zappable addition done by GAN 11/03/86 */
    if (!zappable(obj))
        plines(nothing_happens);
    else if (obj->cursed && !rn2(100)) {
        backfire(obj); /* the wand blows up in your face! */
        exercise(A_STR, false);
        return (1);
    } else if (!(objects[obj->otyp].oc_dir == NODIR) && !getdir((char *)0)) {
        if (!Blind)
            pline("%s glows and fades.", The(xname(obj)));
        /* make him pay for knowing !NODIR */
    } else if (!u.dx && !u.dy && !u.dz && !(objects[obj->otyp].oc_dir == NODIR)) {
        if ((damage = zapyourself(obj, true)) != 0) {
            char buf[BUFSZ];
            sprintf(buf, "zapped %sself with a wand", uhim());
            losehp(damage, buf, NO_KILLER_PREFIX);
        }
    } else {

        /*      Are we having fun yet?
         * weffects -> buzz(obj->otyp) -> zhitm (temple priest) ->
         * attack -> hitum -> known_hitum -> ghod_hitsu ->
         * buzz(AD_ELEC) -> destroy_item(WAND_CLASS) ->
         * useup -> obfree -> dealloc_obj -> free(obj)
         */
        current_wand = obj;
        weffects(obj);
        obj = current_wand;
        current_wand = 0;
    }
    if (obj && obj->spe < 0) {
        char it_turns[BUFSZ];
        Tobjnam(it_turns, BUFSZ, obj, "turn");
        pline("%s to dust.", it_turns);
        useup(obj);
    }
    update_inventory(); /* maybe used a charge */
    return (1);
}

int zapyourself(struct obj *obj, bool ordinary) {
    int damage = 0;
    char buf[BUFSZ];

    switch (obj->otyp) {
        case WAN_STRIKING:
            makeknown(WAN_STRIKING);
        case SPE_FORCE_BOLT:
            if (Antimagic) {
                shieldeff(u.ux, u.uy);
                pline("Boing!");
            } else {
                if (ordinary) {
                    You("bash yourself!");
                    damage = d(2, 12);
                } else
                    damage = d(1 + obj->spe, 6);
                exercise(A_STR, false);
            }
            break;

        case WAN_LIGHTNING:
            makeknown(WAN_LIGHTNING);
            if (!Shock_resistance) {
                You("shock yourself!");
                damage = d(12, 6);
                exercise(A_CON, false);
            } else {
                shieldeff(u.ux, u.uy);
                You("zap yourself, but seem unharmed.");
                ugolemeffects(AD_ELEC, d(12, 6));
            }
            destroy_item(WAND_CLASS, AD_ELEC);
            destroy_item(RING_CLASS, AD_ELEC);
            if (!resists_blnd(&youmonst)) {
                You(are_blinded_by_the_flash);
                make_blinded((long)rnd(100), false);
                if (!Blind)
                    Your("%s", vision_clears);
            }
            break;

        case SPE_FIREBALL:
            You("explode a fireball on top of yourself!");
            explode(u.ux, u.uy, 11, d(6, 6), WAND_CLASS, EXPL_FIERY);
            break;
        case WAN_FIRE:
            makeknown(WAN_FIRE);
        case FIRE_HORN:
            if (Fire_resistance) {
                shieldeff(u.ux, u.uy);
                You_feel("rather warm.");
                ugolemeffects(AD_FIRE, d(12, 6));
            } else {
                pline("You've set yourself afire!");
                damage = d(12, 6);
            }
            burn_away_slime();
            (void)burnarmor(&youmonst);
            destroy_item(SCROLL_CLASS, AD_FIRE);
            destroy_item(POTION_CLASS, AD_FIRE);
            destroy_item(SPBOOK_CLASS, AD_FIRE);
            break;

        case WAN_COLD:
            makeknown(WAN_COLD);
        case SPE_CONE_OF_COLD:
        case FROST_HORN:
            if (Cold_resistance) {
                shieldeff(u.ux, u.uy);
                You_feel("a little chill.");
                ugolemeffects(AD_COLD, d(12, 6));
            } else {
                You("imitate a popsicle!");
                damage = d(12, 6);
            }
            destroy_item(POTION_CLASS, AD_COLD);
            break;

        case WAN_MAGIC_MISSILE:
            makeknown(WAN_MAGIC_MISSILE);
        case SPE_MAGIC_MISSILE:
            if (Antimagic) {
                shieldeff(u.ux, u.uy);
                pline_The("missiles bounce!");
            } else {
                damage = d(4, 6);
                pline("Idiot!  You've shot yourself!");
            }
            break;

        case WAN_POLYMORPH:
            if (!Unchanging)
                makeknown(WAN_POLYMORPH);
        case SPE_POLYMORPH:
            if (!Unchanging)
                polyself(false);
            break;

        case WAN_CANCELLATION:
        case SPE_CANCELLATION:
            (void)cancel_monst(&youmonst, obj, true, false, true);
            break;

        case SPE_DRAIN_LIFE:
            if (!Drain_resistance) {
                losexp("life drainage");
                makeknown(obj->otyp);
            }
            damage = 0; /* No additional damage */
            break;

        case WAN_MAKE_INVISIBLE: {
            /* have to test before changing HInvis but must change
             * HInvis before doing newsym().
             */
            int msg = !Invis && !Blind && !BInvis;

            if (BInvis && uarmc->otyp == MUMMY_WRAPPING) {
                /* A mummy wrapping absorbs it and protects you */
                You_feel("rather itchy under your %s.", xname(uarmc));
                break;
            }
            if (ordinary || !rn2(10)) { /* permanent */
                HInvis |= FROMOUTSIDE;
            } else { /* temporary */
                incr_itimeout(&HInvis, d(obj->spe, 250));
            }
            if (msg) {
                makeknown(WAN_MAKE_INVISIBLE);
                newsym(u.ux, u.uy);
                self_invis_message();
            }
            break;
        }

        case WAN_SPEED_MONSTER:
            if (!(HFast & INTRINSIC)) {
                if (!Fast)
                    You("speed up.");
                else
                    Your("quickness feels more natural.");
                makeknown(WAN_SPEED_MONSTER);
                exercise(A_DEX, true);
            }
            HFast |= FROMOUTSIDE;
            break;

        case WAN_SLEEP:
            makeknown(WAN_SLEEP);
        case SPE_SLEEP:
            if(Sleep_resistance) {
                shieldeff(u.ux, u.uy);
                You("don't feel sleepy!");
            } else {
                pline_The("sleep ray hits you!");
                fall_asleep(-rnd(50), true);
            }
            break;

        case WAN_SLOW_MONSTER:
        case SPE_SLOW_MONSTER:
            if(HFast & (TIMEOUT | INTRINSIC)) {
                u_slow_down();
                makeknown(obj->otyp);
            }
            break;

        case WAN_TELEPORTATION:
        case SPE_TELEPORT_AWAY:
            tele();
            break;

        case WAN_DEATH:
        case SPE_FINGER_OF_DEATH:
            if (nonliving(youmonst.data) || is_demon(youmonst.data)) {
                plines((obj->otyp == WAN_DEATH) ?
                        "The wand shoots an apparently harmless beam at you."
                        : "You seem no deader than before.");
                break;
            }
            sprintf(buf, "shot %sself with a death ray", uhim());
            killer = buf;
            killer_format = NO_KILLER_PREFIX;
            You("irradiate yourself with pure energy!");
            You("die.");
            makeknown(obj->otyp);
            /* They might survive with an amulet of life saving */
            done(DIED);
            break;
        case WAN_UNDEAD_TURNING:
            makeknown(WAN_UNDEAD_TURNING);
        case SPE_TURN_UNDEAD:
            (void) unturn_dead(&youmonst);
            if (is_undead(youmonst.data)) {
                You_feel("frightened and %sstunned.",
                        Stunned ? "even more " : "");
                make_stunned(HStun + rnd(30), false);
            } else
                You("shudder in dread.");
            break;
        case SPE_HEALING:
        case SPE_EXTRA_HEALING:
            healup(d(6, obj->otyp == SPE_EXTRA_HEALING ? 8 : 4),
                    0, false, (obj->otyp == SPE_EXTRA_HEALING));
            You_feel("%sbetter.",
                    obj->otyp == SPE_EXTRA_HEALING ? "much " : "");
            break;
        case WAN_LIGHT: /* (broken wand) */
            /* assert( !ordinary ); */
            damage = d(obj->spe, 25);
        case EXPENSIVE_CAMERA:
            damage += rnd(25);
            if (!resists_blnd(&youmonst)) {
                You(are_blinded_by_the_flash);
                make_blinded((long)damage, false);
                makeknown(obj->otyp);
                if (!Blind) Your("%s", vision_clears);
            }
            damage = 0; /* reset */
            break;
        case WAN_OPENING:
            if (Punished) makeknown(WAN_OPENING);
        case SPE_KNOCK:
            if (Punished) Your("chain quivers for a moment.");
            break;
        case WAN_DIGGING:
        case SPE_DIG:
        case SPE_DETECT_UNSEEN:
        case WAN_NOTHING:
        case WAN_LOCKING:
        case SPE_WIZARD_LOCK:
            break;
        case WAN_PROBING:
            for (obj = invent; obj; obj = obj->nobj)
                obj->dknown = 1;
            /* note: `obj' reused; doesn't point at wand anymore */
            makeknown(WAN_PROBING);
            ustatusline();
            break;
        case SPE_STONE_TO_FLESH:
        {
            struct obj *otemp, *onext;
            bool didmerge;

            if (u.umonnum == PM_STONE_GOLEM)
                (void) polymon(PM_FLESH_GOLEM);
            if (Stoned) fix_petrification(); /* saved! */
            /* but at a cost.. */
            for (otemp = invent; otemp; otemp = onext) {
                onext = otemp->nobj;
                (void) bhito(otemp, obj);
            }
            /*
             * It is possible that we can now merge some inventory.
             * Do a higly paranoid merge.  Restart from the beginning
             * until no merges.
             */
            do {
                didmerge = false;
                for (otemp = invent; !didmerge && otemp; otemp = otemp->nobj)
                    for (onext = otemp->nobj; onext; onext = onext->nobj)
                        if (merged(&otemp, &onext)) {
                            didmerge = true;
                            break;
                        }
            }while (didmerge);
        }
        break;
        default: impossible("object %d used?",obj->otyp);
        break;
    }
    return (damage);
}

/* you've zapped a wand downwards while riding
 * Return true if the steed was hit by the wand.
 * Return false if the steed was not hit by the wand.
 */
/* wand or spell */
static bool zap_steed(struct obj *obj) {
    int steedhit = false;

    switch (obj->otyp) {

        /*
         * Wands that are allowed to hit the steed
         * Carefully test the results of any that are
         * moved here from the bottom section.
         */
        case WAN_PROBING:
            probe_monster(u.usteed);
            makeknown(WAN_PROBING);
            steedhit = true;
            break;
        case WAN_TELEPORTATION:
        case SPE_TELEPORT_AWAY:
            /* you go together */
            tele();
            if (Teleport_control || !couldsee(u.ux0, u.uy0) || (distu(u.ux0, u.uy0) >= 16))
                makeknown(obj->otyp);
            steedhit = true;
            break;

            /* Default processing via bhitm() for these */
        case SPE_CURE_SICKNESS:
        case WAN_MAKE_INVISIBLE:
        case WAN_CANCELLATION:
        case SPE_CANCELLATION:
        case WAN_POLYMORPH:
        case SPE_POLYMORPH:
        case WAN_STRIKING:
        case SPE_FORCE_BOLT:
        case WAN_SLOW_MONSTER:
        case SPE_SLOW_MONSTER:
        case WAN_SPEED_MONSTER:
        case SPE_HEALING:
        case SPE_EXTRA_HEALING:
        case SPE_DRAIN_LIFE:
        case WAN_OPENING:
        case SPE_KNOCK:
            (void)bhitm(u.usteed, obj);
            steedhit = true;
            break;

        default:
            steedhit = false;
            break;
    }
    return steedhit;
}


/*
 * cancel a monster (possibly the hero).  inventory is cancelled only
 * if the monster is zapping itself directly, since otherwise the
 * effect is too strong.  currently non-hero monsters do not zap
 * themselves with cancellation.
 */
bool cancel_monst(struct monst *mdef, struct obj *obj, bool youattack, bool allow_cancel_kill, bool self_cancel) {
    bool youdefend = (mdef == &youmonst);
    static const char your[] = "your"; /* should be extern */

    if (youdefend ? (!youattack && Antimagic) : resist(mdef, obj->oclass, 0, NOTELL))
        return false; /* resisted cancellation */

    if (self_cancel) { /* 1st cancel inventory */
        struct obj *otmp;

        for (otmp = (youdefend ? invent : mdef->minvent); otmp; otmp = otmp->nobj)
            cancel_item(otmp);
        if (youdefend) {
            flags.botl = 1; /* potential AC change */
            find_ac();
        }
    }

    /* now handle special cases */
    if (youdefend) {
        if (Upolyd) {
            if ((u.umonnum == PM_CLAY_GOLEM) && !Blind)
                pline("Some writing vanishes from %s head!", your);

            if (Unchanging)
                Your("amulet grows hot for a moment, then cools.");
            else
                rehumanize();
        }
    } else {
        mdef->mcan = true;

        if (is_were(mdef->data) && mdef->data->mlet != S_HUMAN)
            were_change(mdef);

        if (mdef->data == &mons[PM_CLAY_GOLEM]) {
            if (canseemon(mdef)) {
                char name[BUFSZ];
                mon_nam(name, BUFSZ, mdef);
                pline("Some writing vanishes from %s%s head!", name, possessive_suffix(name));
            }

            if (allow_cancel_kill) {
                if (youattack)
                    killed(mdef);
                else
                    monkilled(mdef, "", AD_SPEL);
            }
        }
    }
    return true;
}

/* you've zapped an immediate type wand up or down */
static bool zap_updown(struct obj *obj /* wand or spell */
) {
    bool striking = false, disclose = false;
    int x, y, xx, yy, ptmp;
    struct obj *otmp;
    struct engr *e;
    struct trap *ttmp;
    char buf[BUFSZ];

    /* some wands have special effects other than normal bhitpile */
    /* drawbridge might change <u.ux,u.uy> */
    x = xx = u.ux; /* <x,y> is zap location */
    y = yy = u.uy; /* <xx,yy> is drawbridge (portcullis) position */
    ttmp = t_at(x, y); /* trap if there is one */

    switch (obj->otyp) {
        case WAN_PROBING:
            ptmp = 0;
            if (u.dz < 0) {
                You("probe towards the %s.", ceiling(x, y));
            } else {
                ptmp += bhitpile(obj, bhito, x, y);
                You("probe beneath the %s.", surface(x, y));
                ptmp += display_binventory(x, y, true);
            }
            if (!ptmp)
                Your("probe reveals nothing.");
            return true; /* we've done our own bhitpile */
        case WAN_OPENING:
        case SPE_KNOCK:
            /* up or down, but at closed portcullis only */
            if (is_db_wall(x, y) && find_drawbridge(&xx, &yy)) {
                open_drawbridge(xx, yy);
                disclose = true;
            } else if (u.dz > 0 && (x == xdnstair && y == ydnstair) &&
            /* can't use the stairs down to quest level 2 until
             leader "unlocks" them; give feedback if you try */
            on_level(&u.uz, &qstart_level) && !ok_to_quest()) {
                pline_The("stairs seem to ripple momentarily.");
                disclose = true;
            }
            break;
        case WAN_STRIKING:
        case SPE_FORCE_BOLT:
            striking = true;
            /*FALLTHRU*/
        case WAN_LOCKING:
        case SPE_WIZARD_LOCK:
            /* down at open bridge or up or down at open portcullis */
            if ((levl[x][y].typ == DRAWBRIDGE_DOWN) ? (u.dz > 0) : (is_drawbridge_wall(x, y) && !is_db_wall(x, y)) && find_drawbridge(&xx, &yy)) {
                if (!striking)
                    close_drawbridge(xx, yy);
                else
                    destroy_drawbridge(xx, yy);
                disclose = true;
            } else if (striking && u.dz < 0 && rn2(3) && !Is_airlevel(&u.uz) && !Is_waterlevel(&u.uz) && !Underwater && !Is_qstart(&u.uz)) {
                /* similar to zap_dig() */
                pline("A rock is dislodged from the %s and falls on your %s.", ceiling(x, y), body_part(HEAD));
                losehp(rnd((uarmh && is_metallic(uarmh)) ? 2 : 6), "falling rock", KILLED_BY_AN);
                if ((otmp = mksobj_at(ROCK, x, y, false, false)) != 0) {
                    (void)xname(otmp); /* set dknown, maybe bknown */
                    stackobj(otmp);
                }
                newsym(x, y);
            } else if (!striking && ttmp && ttmp->ttyp == TRAPDOOR && u.dz > 0) {
                if (!Blind) {
                    if (ttmp->tseen) {
                        pline("A trap door beneath you closes up then vanishes.");
                        disclose = true;
                    } else {
                        You("see a swirl of %s beneath you.", is_ice(x, y) ? "frost" : "dust");
                    }
                } else {
                    You_hear("a twang followed by a thud.");
                }
                deltrap(ttmp);
                ttmp = (struct trap *)0;
                newsym(x, y);
            }
            break;
        case SPE_STONE_TO_FLESH:
            if (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz) ||
            Underwater || (Is_qstart(&u.uz) && u.dz < 0)) {
                plines(nothing_happens);
            } else if (u.dz < 0) { /* we should do more... */
                pline("Blood drips on your %s.", body_part(FACE));
            } else if (u.dz > 0 && !OBJ_AT(u.ux, u.uy)) {
                /*
                 Print this message only if there wasn't an engraving
                 affected here.  If water or ice, act like waterlevel case.
                 */
                e = engr_at(u.ux, u.uy);
                if (!(e && e->engr_type == ENGRAVE)) {
                    if (is_pool(u.ux, u.uy) || is_ice(u.ux, u.uy))
                        plines(nothing_happens);
                    else
                        pline("Blood %ss %s your %s.", is_lava(u.ux, u.uy) ? "boil" : "pool",
                        Levitation ? "beneath" : "at", makeplural(body_part(FOOT)));
                }
            }
            break;
        default:
            break;
    }

    if (u.dz > 0) {
        /* zapping downward */
        (void)bhitpile(obj, bhito, x, y);

        /* subset of engraving effects; none sets `disclose' */
        if ((e = engr_at(x, y)) != 0 && e->engr_type != HEADSTONE) {
            switch (obj->otyp) {
                case WAN_POLYMORPH:
                case SPE_POLYMORPH:
                    del_engr(e);
                    make_engr_at(x, y, random_engraving(buf), moves, (signed char)0);
                    break;
                case WAN_CANCELLATION:
                case SPE_CANCELLATION:
                case WAN_MAKE_INVISIBLE:
                    del_engr(e);
                    break;
                case WAN_TELEPORTATION:
                case SPE_TELEPORT_AWAY:
                    rloc_engr(e);
                    break;
                case SPE_STONE_TO_FLESH:
                    if (e->engr_type == ENGRAVE) {
                        /* only affects things in stone */
                        pline_The(Hallucination() ? "floor runs like butter!" : "edges on the floor get smoother.");
                        wipe_engr_at(x, y, d(2, 4));
                    }
                    break;
                case WAN_STRIKING:
                case SPE_FORCE_BOLT:
                    wipe_engr_at(x, y, d(2, 4));
                    break;
                default:
                    break;
            }
        }
    }

    return disclose;
}


/* called for various wand and spell effects - M. Stephenson */
void weffects(struct obj *obj) {
    int otyp = obj->otyp;
    bool disclose = false, was_unkn = !objects[otyp].oc_name_known;

    exercise(A_WIS, true);
    if (u.usteed && (objects[otyp].oc_dir != NODIR) && !u.dx && !u.dy && (u.dz > 0) && zap_steed(obj)) {
        disclose = true;
    } else if (objects[otyp].oc_dir == IMMEDIATE) {
        obj_zapped = false;

        if (u.uswallow) {
            (void)bhitm(u.ustuck, obj);
            /* [how about `bhitpile(u.ustuck->minvent)' effect?] */
        } else if (u.dz) {
            disclose = zap_updown(obj);
        } else {
            (void)bhit(u.dx, u.dy, rn1(8, 6), ZAPPED_WAND, bhitm, bhito, obj);
        }
        /* give a clue if obj_zapped */
        if (obj_zapped)
            You_feel("shuddering vibrations.");

    } else if (objects[otyp].oc_dir == NODIR) {
        zapnodir(obj);

    } else {
        /* neither immediate nor directionless */

        if (otyp == WAN_DIGGING || otyp == SPE_DIG)
            zap_dig();
        else if (otyp >= SPE_MAGIC_MISSILE && otyp <= SPE_FINGER_OF_DEATH)
            buzz(otyp - SPE_MAGIC_MISSILE + 10, u.ulevel / 2 + 1, u.ux, u.uy, u.dx, u.dy);
        else if (otyp >= WAN_MAGIC_MISSILE && otyp <= WAN_LIGHTNING)
            buzz(otyp - WAN_MAGIC_MISSILE, (otyp == WAN_MAGIC_MISSILE) ? 2 : 6, u.ux, u.uy, u.dx, u.dy);
        else
            impossible("weffects: unexpected spell or wand");
        disclose = true;
    }
    if (disclose && was_unkn) {
        makeknown(otyp);
        more_experienced(0, 10);
    }
    return;
}

/*
 * Generate the to damage bonus for a spell. Based on the hero's intelligence
 */
int spell_damage_bonus(void) {
    int intell = ACURR(A_INT);

    /* Punish low intellegence before low level else low intellegence
     gets punished only when high level */
    if (intell < 10)
        return -3;
    else if (u.ulevel < 5)
        return 0;
    else if (intell < 14)
        return 0;
    else if (intell <= 18)
        return 1;
    else
        return 2; /* helm of brilliance */
}

/*
 * Generate the to hit bonus for a spell.  Based on the hero's skill in
 * spell class and dexterity.
 */
static int spell_hit_bonus(int skill) {
    int hit_bon = 0;
    int dex = ACURR(A_DEX);

    switch (P_SKILL(spell_skilltype(skill))) {
        case P_ISRESTRICTED:
        case P_UNSKILLED:
            hit_bon = -4;
            break;
        case P_BASIC:
            hit_bon = 0;
            break;
        case P_SKILLED:
            hit_bon = 2;
            break;
        case P_EXPERT:
            hit_bon = 3;
            break;
    }

    if (dex < 4)
        hit_bon -= 3;
    else if (dex < 6)
        hit_bon -= 2;
    else if (dex < 8)
        hit_bon -= 1;
    else if (dex < 14)
        hit_bon -= 0; /* Will change when print stuff below removed */
    else
        hit_bon += dex - 14; /* Even increment for dextrous heroes (see weapon.c abon) */

    return hit_bon;
}

const char * exclam(int force) {
    /* force == 0 occurs e.g. with sleep ray */
    /* note that large force is usual with wands so that !! would
     require information about hand/weapon/wand */
    return (const char *)((force < 0) ? "?" : (force <= 4) ? "." : "!");
}

/* force is usually either "." or "!" */
void hit(const char *str, struct monst *mtmp, const char *force) {
    char hits[BUFSZ];
    vtense(hits, BUFSZ, str, "hit");
    if ((!cansee(bhitpos.x, bhitpos.y) && !canspotmon(mtmp) && !(u.uswallow && mtmp == u.ustuck)) || !flags.verbose) {
        pline("%s %s it.", The(str), hits);
    } else {
        char direct_object[BUFSZ];
        mon_nam(direct_object, BUFSZ, mtmp);
        pline("%s %s %s%s", The(str), hits, direct_object, force);
    }
}

void miss(const char *str, struct monst *mtmp) {
    char subject[BUFSZ];
    strcpy(subject, The(str));

    char verb[BUFSZ];
    vtense(verb, BUFSZ, str, "miss");

    char direct_object[BUFSZ];
    if ((cansee(bhitpos.x,bhitpos.y) || canspotmon(mtmp)) && flags.verbose)
        mon_nam(direct_object, BUFSIZ, mtmp);
    else
        strcpy(direct_object, "it");

    pline("%s %s %s.", subject, verb, direct_object);
}

/*
 *  Called for the following distance effects:
 *      when a weapon is thrown (weapon == THROWN_WEAPON)
 *      when an object is kicked (KICKED_WEAPON)
 *      when an IMMEDIATE wand is zapped (ZAPPED_WAND)
 *      when a light beam is flashed (FLASHED_LIGHT)
 *      when a mirror is applied (INVIS_BEAM)
 *  A thrown/kicked object falls down at the end of its range or when a monster
 *  is hit.  The variable 'bhitpos' is set to the final position of the weapon
 *  thrown/zapped.  The ray of a wand may affect (by calling a provided
 *  function) several objects and monsters on its path.  The return value
 *  is the monster hit (weapon != ZAPPED_WAND), or a null monster pointer.
 *
 *  Check !u.uswallow before calling bhit().
 *  This function reveals the absence of a remembered invisible monster in
 *  necessary cases (throwing or kicking weapons).  The presence of a real
 *  one is revealed for a weapon, but if not a weapon is left up to fhitm().
 */
/* range: direction and range */
/* weapon: see values in hack.h */
/* fhitm, fhito: fns called when mon/obj hit */
/* obj: object tossed/used */
struct monst * bhit(int ddx, int ddy, int range, int weapon, int (*fhitm)(struct monst *, struct obj *), int (*fhito)(struct obj *, struct obj *), struct obj *obj) {
    struct monst *mtmp;
    unsigned char typ;
    bool shopdoor = false, point_blank = true;

    if (weapon == KICKED_WEAPON) {
        /* object starts one square in front of player */
        bhitpos.x = u.ux + ddx;
        bhitpos.y = u.uy + ddy;
        range--;
    } else {
        bhitpos.x = u.ux;
        bhitpos.y = u.uy;
    }

    if (weapon == FLASHED_LIGHT) {
        tmp_at(DISP_BEAM, cmap_to_glyph(S_flashbeam));
    } else if (weapon != ZAPPED_WAND && weapon != INVIS_BEAM)
        tmp_at(DISP_FLASH, obj_to_glyph(obj));

    while (range-- > 0) {
        int x, y;

        bhitpos.x += ddx;
        bhitpos.y += ddy;
        x = bhitpos.x;
        y = bhitpos.y;

        if (!isok(x, y)) {
            bhitpos.x -= ddx;
            bhitpos.y -= ddy;
            break;
        }

        if (is_pick(obj) && inside_shop(x, y) && (mtmp = shkcatch(obj, x, y))) {
            tmp_at(DISP_END, 0);
            return (mtmp);
        }

        typ = levl[bhitpos.x][bhitpos.y].typ;

        /* iron bars will block anything big enough */
        if ((weapon == THROWN_WEAPON || weapon == KICKED_WEAPON) && typ == IRONBARS && hits_bars(&obj, x - ddx, y - ddy, point_blank ? 0 : !rn2(5), 1)) {
            /* caveat: obj might now be null... */
            bhitpos.x -= ddx;
            bhitpos.y -= ddy;
            break;
        }

        if (weapon == ZAPPED_WAND && find_drawbridge(&x, &y)) {
            switch (obj->otyp) {
                case WAN_OPENING:
                case SPE_KNOCK:
                    if (is_db_wall(bhitpos.x, bhitpos.y)) {
                        if (cansee(x,y) || cansee(bhitpos.x, bhitpos.y))
                            makeknown(obj->otyp);
                        open_drawbridge(x, y);
                    }
                    break;
                case WAN_LOCKING:
                case SPE_WIZARD_LOCK:
                    if ((cansee(x,y) || cansee(bhitpos.x, bhitpos.y)) && levl[x][y].typ == DRAWBRIDGE_DOWN)
                        makeknown(obj->otyp);
                    close_drawbridge(x, y);
                    break;
                case WAN_STRIKING:
                case SPE_FORCE_BOLT:
                    if (typ != DRAWBRIDGE_UP)
                        destroy_drawbridge(x, y);
                    makeknown(obj->otyp);
                    break;
            }
        }

        if ((mtmp = m_at(bhitpos.x, bhitpos.y)) != 0) {
            notonhead = (bhitpos.x != mtmp->mx || bhitpos.y != mtmp->my);
            if (weapon != FLASHED_LIGHT) {
                if (weapon != ZAPPED_WAND) {
                    if (weapon != INVIS_BEAM)
                        tmp_at(DISP_END, 0);
                    if (cansee(bhitpos.x,bhitpos.y) && !canspotmon(mtmp)) {
                        if (weapon != INVIS_BEAM) {
                            map_invisible(bhitpos.x, bhitpos.y);
                            return (mtmp);
                        }
                    } else
                        return (mtmp);
                }
                if (weapon != INVIS_BEAM) {
                    (*fhitm)(mtmp, obj);
                    range -= 3;
                }
            } else {
                /* FLASHED_LIGHT hitting invisible monster
                 should pass through instead of stop so
                 we call flash_hits_mon() directly rather
                 than returning mtmp back to caller. That
                 allows the flash to keep on going. Note
                 that we use mtmp->minvis not canspotmon()
                 because it makes no difference whether
                 the hero can see the monster or not.*/
                if (mtmp->minvis) {
                    obj->ox = u.ux, obj->oy = u.uy;
                    (void)flash_hits_mon(mtmp, obj);
                } else {
                    tmp_at(DISP_END, 0);
                    return (mtmp); /* caller will call flash_hits_mon */
                }
            }
        } else {
            if (weapon == ZAPPED_WAND && obj->otyp == WAN_PROBING && glyph_is_invisible(levl[bhitpos.x][bhitpos.y].glyph)) {
                unmap_object(bhitpos.x, bhitpos.y);
                newsym(x, y);
            }
        }
        if (fhito) {
            if (bhitpile(obj, fhito, bhitpos.x, bhitpos.y))
                range--;
        } else {
            if (weapon == KICKED_WEAPON && ((obj->oclass == COIN_CLASS && OBJ_AT(bhitpos.x, bhitpos.y)) || ship_object(obj, bhitpos.x, bhitpos.y, costly_spot(bhitpos.x, bhitpos.y)))) {
                tmp_at(DISP_END, 0);
                return (struct monst *)0;
            }
        }
        if (weapon == ZAPPED_WAND && (IS_DOOR(typ) || typ == SDOOR)) {
            switch (obj->otyp) {
                case WAN_OPENING:
                case WAN_LOCKING:
                case WAN_STRIKING:
                case SPE_KNOCK:
                case SPE_WIZARD_LOCK:
                case SPE_FORCE_BOLT:
                    if (doorlock(obj, bhitpos.x, bhitpos.y)) {
                        if (cansee(bhitpos.x, bhitpos.y) || (obj->otyp == WAN_STRIKING))
                            makeknown(obj->otyp);
                        if (levl[bhitpos.x][bhitpos.y].doormask == D_BROKEN && *in_rooms(bhitpos.x, bhitpos.y, SHOPBASE)) {
                            shopdoor = true;
                            add_damage(bhitpos.x, bhitpos.y, 400L);
                        }
                    }
                    break;
            }
        }
        if (!ZAP_POS(typ) || closed_door(bhitpos.x, bhitpos.y)) {
            bhitpos.x -= ddx;
            bhitpos.y -= ddy;
            break;
        }
        if (weapon != ZAPPED_WAND && weapon != INVIS_BEAM) {
            /* 'I' present but no monster: erase */
            /* do this before the tmp_at() */
            if (glyph_is_invisible(levl[bhitpos.x][bhitpos.y].glyph) && cansee(x, y)) {
                unmap_object(bhitpos.x, bhitpos.y);
                newsym(x, y);
            }
            tmp_at(bhitpos.x, bhitpos.y);
            /* kicked objects fall in pools */
            if ((weapon == KICKED_WEAPON) && (is_pool(bhitpos.x, bhitpos.y) || is_lava(bhitpos.x, bhitpos.y)))
                break;
            if (IS_SINK(typ) && weapon != FLASHED_LIGHT)
                break; /* physical objects fall onto sink */
        }
        /* limit range of ball so hero won't make an invalid move */
        if (weapon == THROWN_WEAPON && range > 0 && obj->otyp == HEAVY_IRON_BALL) {
            struct obj *bobj;
            struct trap *t;
            if ((bobj = sobj_at(BOULDER, x, y)) != 0) {
                if (cansee(x, y))
                    pline("%s hits %s.", The(distant_name(obj, xname)), an(xname(bobj)));
                range = 0;
            } else if (obj == uball) {
                if (!test_move(x - ddx, y - ddy, ddx, ddy, TEST_MOVE)) {
                    /* nb: it didn't hit anything directly */
                    if (cansee(x, y))
                        pline("%s jerks to an abrupt halt.", The(distant_name(obj, xname))); /* lame */
                    range = 0;
                } else if (In_sokoban(&u.uz) && (t = t_at(x, y)) != 0 && (t->ttyp == PIT || t->ttyp == SPIKED_PIT || t->ttyp == HOLE || t->ttyp == TRAPDOOR)) {
                    /* hero falls into the trap, so ball stops */
                    range = 0;
                }
            }
        }

        /* thrown/kicked missile has moved away from its starting spot */
        point_blank = false; /* affects passing through iron bars */
    }

    if (weapon != ZAPPED_WAND && weapon != INVIS_BEAM)
        tmp_at(DISP_END, 0);

    if (shopdoor)
        pay_for_damage("destroy", false);

    return (struct monst *)0;
}

struct monst * boomhit(int dx, int dy) {
    int i, ct;
    int boom = S_boomleft; /* showsym[] index  */
    struct monst *mtmp;

    bhitpos.x = u.ux;
    bhitpos.y = u.uy;

    for (i = 0; i < 8; i++)
        if (xdir[i] == dx && ydir[i] == dy)
            break;
    tmp_at(DISP_FLASH, cmap_to_glyph(boom));
    for (ct = 0; ct < 10; ct++) {
        if (i == 8)
            i = 0;
        boom = (boom == S_boomleft) ? S_boomright : S_boomleft;
        tmp_at(DISP_CHANGE, cmap_to_glyph(boom));/* change glyph */
        dx = xdir[i];
        dy = ydir[i];
        bhitpos.x += dx;
        bhitpos.y += dy;
        if (MON_AT(bhitpos.x, bhitpos.y)) {
            mtmp = m_at(bhitpos.x, bhitpos.y);
            m_respond(mtmp);
            tmp_at(DISP_END, 0);
            return (mtmp);
        }
        if (!ZAP_POS(levl[bhitpos.x][bhitpos.y].typ) || closed_door(bhitpos.x, bhitpos.y)) {
            bhitpos.x -= dx;
            bhitpos.y -= dy;
            break;
        }
        if (bhitpos.x == u.ux && bhitpos.y == u.uy) { /* ct == 9 */
            if (Fumbling || rn2(20) >= ACURR(A_DEX)) {
                /* we hit ourselves */
                (void)thitu(10, rnd(10), (struct obj *)0, "boomerang");
                break;
            } else { /* we catch it */
                tmp_at(DISP_END, 0);
                You("skillfully catch the boomerang.");
                return (&youmonst);
            }
        }
        tmp_at(bhitpos.x, bhitpos.y);
        if (ct % 5 != 0)
            i++;
        if (IS_SINK(levl[bhitpos.x][bhitpos.y].typ))
            break; /* boomerang falls on sink */
    }
    tmp_at(DISP_END, 0); /* do not leave last symbol */
    return (struct monst *)0;
}

/* returns damage to mon */
/* ootmp: to return worn armor for caller to disintegrate */
static int zhitm(struct monst *mon, int type, int nd, struct obj **ootmp) {
    int tmp = 0;
    int abstype = abs(type) % 10;
    bool sho_shieldeff = false;
    bool spellcaster = is_hero_spell(type); /* maybe get a bonus! */

    *ootmp = (struct obj *)0;
    switch (abstype) {
        case ZT_MAGIC_MISSILE:
            if (resists_magm(mon)) {
                sho_shieldeff = true;
                break;
            }
            tmp = d(nd, 6);
            if (spellcaster)
                tmp += spell_damage_bonus();
            break;
        case ZT_FIRE:
            if (resists_fire(mon)) {
                sho_shieldeff = true;
                break;
            }
            tmp = d(nd, 6);
            if (resists_cold(mon))
                tmp += 7;
            if (spellcaster)
                tmp += spell_damage_bonus();
            if (burnarmor(mon)) {
                if (!rn2(3))
                    (void)destroy_mitem(mon, POTION_CLASS, AD_FIRE);
                if (!rn2(3))
                    (void)destroy_mitem(mon, SCROLL_CLASS, AD_FIRE);
                if (!rn2(5))
                    (void)destroy_mitem(mon, SPBOOK_CLASS, AD_FIRE);
            }
            break;
        case ZT_COLD:
            if (resists_cold(mon)) {
                sho_shieldeff = true;
                break;
            }
            tmp = d(nd, 6);
            if (resists_fire(mon))
                tmp += d(nd, 3);
            if (spellcaster)
                tmp += spell_damage_bonus();
            if (!rn2(3))
                (void)destroy_mitem(mon, POTION_CLASS, AD_COLD);
            break;
        case ZT_SLEEP:
            tmp = 0;
            (void)sleep_monst(mon, d(nd, 25), type == ZT_WAND(ZT_SLEEP) ? WAND_CLASS : '\0');
            break;
        case ZT_DEATH: /* death/disintegration */
            if (abs(type) != ZT_BREATH(ZT_DEATH)) { /* death */
                if (mon->data == &mons[PM_DEATH]) {
                    mon->mhpmax += mon->mhpmax / 2;
                    if (mon->mhpmax >= MAGIC_COOKIE)
                        mon->mhpmax = MAGIC_COOKIE - 1;
                    mon->mhp = mon->mhpmax;
                    tmp = 0;
                    break;
                }
                if (nonliving(mon->data) || is_demon(mon->data) || resists_magm(mon)) { /* similar to player */
                    sho_shieldeff = true;
                    break;
                }
                type = -1; /* so they don't get saving throws */
            } else {
                struct obj *otmp2;

                if (resists_disint(mon)) {
                    sho_shieldeff = true;
                } else if (mon->misc_worn_check & W_ARMS) {
                    /* destroy shield; victim survives */
                    *ootmp = which_armor(mon, W_ARMS);
                } else if (mon->misc_worn_check & W_ARM) {
                    /* destroy body armor, also cloak if present */
                    *ootmp = which_armor(mon, W_ARM);
                    if ((otmp2 = which_armor(mon, W_ARMC)) != 0)
                        m_useup(mon, otmp2);
                } else {
                    /* no body armor, victim dies; destroy cloak
                     and shirt now in case target gets life-saved */
                    tmp = MAGIC_COOKIE;
                    if ((otmp2 = which_armor(mon, W_ARMC)) != 0)
                        m_useup(mon, otmp2);
                    if ((otmp2 = which_armor(mon, W_ARMU)) != 0)
                        m_useup(mon, otmp2);
                }
                type = -1; /* no saving throw wanted */
                break; /* not ordinary damage */
            }
            tmp = mon->mhp + 1;
            break;
        case ZT_LIGHTNING:
            if (resists_elec(mon)) {
                sho_shieldeff = true;
                tmp = 0;
                /* can still blind the monster */
            } else
                tmp = d(nd, 6);
            if (spellcaster)
                tmp += spell_damage_bonus();
            if (!resists_blnd(mon) && !(type > 0 && u.uswallow && mon == u.ustuck)) {
                unsigned rnd_tmp = rnd(50);
                mon->mcansee = 0;
                if ((mon->mblinded + rnd_tmp) > 127)
                    mon->mblinded = 127;
                else
                    mon->mblinded += rnd_tmp;
            }
            if (!rn2(3))
                (void)destroy_mitem(mon, WAND_CLASS, AD_ELEC);
            /* not actually possible yet */
            if (!rn2(3))
                (void)destroy_mitem(mon, RING_CLASS, AD_ELEC);
            break;
        case ZT_POISON_GAS:
            if (resists_poison(mon)) {
                sho_shieldeff = true;
                break;
            }
            tmp = d(nd, 6);
            break;
        case ZT_ACID:
            if (resists_acid(mon)) {
                sho_shieldeff = true;
                break;
            }
            tmp = d(nd, 6);
            if (!rn2(6))
                erode_obj(MON_WEP(mon), true, true);
            if (!rn2(6))
                erode_armor(mon, true);
            break;
    }
    if (sho_shieldeff)
        shieldeff(mon->mx, mon->my);
    if (is_hero_spell(type) && (Role_if(PM_KNIGHT) && u.uhave.questart))
        tmp *= 2;
    if (tmp > 0 && type >= 0 && resist(mon, type < ZT_SPELL(0) ? WAND_CLASS : '\0', 0, NOTELL))
        tmp /= 2;
    if (tmp < 0)
        tmp = 0; /* don't allow negative damage */
    mon->mhp -= tmp;
    return (tmp);
}

static void zhitu(int type, int nd, const char *fltxt, signed char sx, signed char sy) {
    int dam = 0;

    switch (abs(type) % 10) {
        case ZT_MAGIC_MISSILE:
            if (Antimagic) {
                shieldeff(sx, sy);
                pline_The("missiles bounce off!");
            } else {
                dam = d(nd, 6);
                exercise(A_STR, false);
            }
            break;
        case ZT_FIRE:
            if (Fire_resistance) {
                shieldeff(sx, sy);
                You("don't feel hot!");
                ugolemeffects(AD_FIRE, d(nd, 6));
            } else {
                dam = d(nd, 6);
            }
            burn_away_slime();
            if (burnarmor(&youmonst)) { /* "body hit" */
                if (!rn2(3))
                    destroy_item(POTION_CLASS, AD_FIRE);
                if (!rn2(3))
                    destroy_item(SCROLL_CLASS, AD_FIRE);
                if (!rn2(5))
                    destroy_item(SPBOOK_CLASS, AD_FIRE);
            }
            break;
        case ZT_COLD:
            if (Cold_resistance) {
                shieldeff(sx, sy);
                You("don't feel cold.");
                ugolemeffects(AD_COLD, d(nd, 6));
            } else {
                dam = d(nd, 6);
            }
            if (!rn2(3))
                destroy_item(POTION_CLASS, AD_COLD);
            break;
        case ZT_SLEEP:
            if (Sleep_resistance) {
                shieldeff(u.ux, u.uy);
                You("don't feel sleepy.");
            } else {
                fall_asleep(-d(nd, 25), true); /* sleep ray */
            }
            break;
        case ZT_DEATH:
            if (abs(type) == ZT_BREATH(ZT_DEATH)) {
                if (Disint_resistance) {
                    You("are not disintegrated.");
                    break;
                } else if (uarms) {
                    /* destroy shield; other possessions are safe */
                    (void)destroy_arm(uarms);
                    break;
                } else if (uarm) {
                    /* destroy suit; if present, cloak goes too */
                    if (uarmc)
                        (void)destroy_arm(uarmc);
                    (void)destroy_arm(uarm);
                    break;
                }
                /* no shield or suit, you're dead; wipe out cloak
                 and/or shirt in case of life-saving or bones */
                if (uarmc)
                    (void)destroy_arm(uarmc);
                if (uarmu)
                    (void)destroy_arm(uarmu);
            } else if (nonliving(youmonst.data) || is_demon(youmonst.data)) {
                shieldeff(sx, sy);
                You("seem unaffected.");
                break;
            } else if (Antimagic) {
                shieldeff(sx, sy);
                You("aren't affected.");
                break;
            }
            killer_format = KILLED_BY_AN;
            killer = fltxt;
            /* when killed by disintegration breath, don't leave corpse */
            u.ugrave_arise = (type == -ZT_BREATH(ZT_DEATH)) ? -3 : NON_PM;
            done(DIED);
            return; /* lifesaved */
        case ZT_LIGHTNING:
            if (Shock_resistance) {
                shieldeff(sx, sy);
                You("aren't affected.");
                ugolemeffects(AD_ELEC, d(nd, 6));
            } else {
                dam = d(nd, 6);
                exercise(A_CON, false);
            }
            if (!rn2(3))
                destroy_item(WAND_CLASS, AD_ELEC);
            if (!rn2(3))
                destroy_item(RING_CLASS, AD_ELEC);
            break;
        case ZT_POISON_GAS:
            poisoned("blast", A_DEX, "poisoned blast", 15);
            break;
        case ZT_ACID:
            if (Acid_resistance) {
                dam = 0;
            } else {
                pline_The("acid burns!");
                dam = d(nd, 6);
                exercise(A_STR, false);
            }
            /* using two weapons at once makes both of them more vulnerable */
            if (!rn2(u.twoweap ? 3 : 6))
                erode_obj(uwep, true, true);
            if (u.twoweap && !rn2(3))
                erode_obj(uswapwep, true, true);
            if (!rn2(6))
                erode_armor(&youmonst, true);
            break;
    }

    if (Half_spell_damage && dam && type < 0 && (type > -20 || type < -29)) /* !Breath */
        dam = (dam + 1) / 2;
    losehp(dam, fltxt, KILLED_BY_AN);
    return;
}


/*
 * burn scrolls and spellbooks on floor at position x,y
 * return the number of scrolls and spellbooks burned
 */
/* give_feedback: caller needs to decide about visibility checks */
int burn_floor_paper(int x, int y,
bool give_feedback,
bool u_caused) {
    struct obj *obj, *obj2;
    long i, scrquan, delquan;
    char buf1[BUFSZ], buf2[BUFSZ];
    int cnt = 0;

    for (obj = level.objects[x][y]; obj; obj = obj2) {
        obj2 = obj->nexthere;
        if (obj->oclass == SCROLL_CLASS || obj->oclass == SPBOOK_CLASS) {
            if (obj->otyp == SCR_FIRE || obj->otyp == SPE_FIREBALL || obj_resists(obj, 2, 100))
                continue;
            scrquan = obj->quan; /* number present */
            delquan = 0; /* number to destroy */
            for (i = scrquan; i > 0; i--)
                if (!rn2(3))
                    delquan++;
            if (delquan) {
                /* save name before potential delobj() */
                if (give_feedback) {
                    obj->quan = 1;
                    strcpy(buf1, (x == u.ux && y == u.uy) ? xname(obj) : distant_name(obj, xname));
                    obj->quan = 2;
                    strcpy(buf2, (x == u.ux && y == u.uy) ? xname(obj) : distant_name(obj, xname));
                    obj->quan = scrquan;
                }
                /* useupf(), which charges, only if hero caused damage */
                if (u_caused)
                    useupf(obj, delquan);
                else if (delquan < scrquan)
                    obj->quan -= delquan;
                else
                    delobj(obj);
                cnt += delquan;
                if (give_feedback) {
                    if (delquan > 1)
                        pline("%ld %s burn.", delquan, buf2);
                    else
                        pline("%s burns.", An(buf1));
                }
            }
        }
    }
    return cnt;
}

/* will zap/spell/breath attack score a hit against armor class `ac'? */
/* type: either hero cast spell type or 0 */
static int zap_hit(int ac, int type) {
    int chance = rn2(20);
    int spell_bonus = type ? spell_hit_bonus(type) : 0;

    /* small chance for naked target to avoid being hit */
    if (!chance)
        return rnd(10) < ac + spell_bonus;

    /* very high armor protection does not achieve invulnerability */
    ac = AC_VALUE(ac);

    return (3 - chance) < ac + spell_bonus;
}

/* type ==   0 to   9 : you shooting a wand */
/* type ==  10 to  19 : you casting a spell */
/* type ==  20 to  29 : you breathing as a monster */
/* type == -10 to -19 : monster casting spell */
/* type == -20 to -29 : monster breathing at you */
/* type == -30 to -39 : monster shooting a wand */
/* called with dx = dy = 0 with vertical bolts */
void buzz(int type, int nd, signed char sx, signed char sy, int dx, int dy) {
    int range, abstype = abs(type) % 10;
    struct rm *lev;
    signed char lsx, lsy;
    struct monst *mon;
    coord save_bhitpos;
    bool shopdamage = false;
    const char *fltxt;
    struct obj *otmp;
    int spell_type;

    /* if its a Hero Spell then get its SPE_TYPE */
    spell_type = is_hero_spell(type) ? SPE_MAGIC_MISSILE + abstype : 0;

    fltxt = flash_types[(type <= -30) ? abstype : abs(type)];
    if (u.uswallow) {
        if (type < 0)
            return;
        int tmp = zhitm(u.ustuck, type, nd, &otmp);
        if (!u.ustuck) {
            u.uswallow = 0;
        } else {
            char name[BUFSZ];
            mon_nam(name, BUFSZ, u.ustuck);
            pline("%s rips into %s%s", The(fltxt), name, exclam(tmp));
        }
        /* Using disintegration from the inside only makes a hole... */
        if (tmp == MAGIC_COOKIE)
            u.ustuck->mhp = 0;
        if (u.ustuck->mhp < 1)
            killed(u.ustuck);
        return;
    }
    if (type < 0)
        newsym(u.ux, u.uy);
    range = rn1(7, 7);
    if (dx == 0 && dy == 0)
        range = 1;
    save_bhitpos = bhitpos;

    tmp_at(DISP_BEAM, zapdir_to_glyph(dx, dy, abstype));
    while (range-- > 0) {
        lsx = sx;
        sx += dx;
        lsy = sy;
        sy += dy;
        if (isok(sx, sy) && (lev = &levl[sx][sy])->typ) {
            mon = m_at(sx, sy);
            if (cansee(sx, sy)) {
                /* reveal/unreveal invisible monsters before tmp_at() */
                if (mon && !canspotmon(mon))
                    map_invisible(sx, sy);
                else if (!mon && glyph_is_invisible(levl[sx][sy].glyph)) {
                    unmap_object(sx, sy);
                    newsym(sx, sy);
                }
                if (ZAP_POS(lev->typ) || cansee(lsx, lsy))
                    tmp_at(sx, sy);
            }
        } else
            goto make_bounce;

        /* hit() and miss() need bhitpos to match the target */
        bhitpos.x = sx, bhitpos.y = sy;
        /* Fireballs only damage when they explode */
        if (type != ZT_SPELL(ZT_FIRE))
            range += zap_over_floor(sx, sy, type, &shopdamage);

        if (mon) {
            if (type == ZT_SPELL(ZT_FIRE))
                break;
            if (type >= 0)
                mon->mstrategy &= ~STRAT_WAITMASK;
            buzzmonst: if (zap_hit(find_mac(mon), spell_type)) {
                if (mon_reflects(mon, (char *)0)) {
                    if (cansee(mon->mx, mon->my)) {
                        hit(fltxt, mon, exclam(0));
                        shieldeff(mon->mx, mon->my);
                        (void)mon_reflects(mon, "But it reflects from %s %s!");
                    }
                    dx = -dx;
                    dy = -dy;
                } else {
                    bool mon_could_move = mon->mcanmove;
                    int tmp = zhitm(mon, type, nd, &otmp);

                    if (is_rider(mon->data) && abs(type) == ZT_BREATH(ZT_DEATH)) {
                        if (canseemon(mon)) {
                            hit(fltxt, mon, ".");
                            char name[BUFSZ];
                            Monnam(name, BUFSZ, mon);
                            char your_eyes[BUFSZ];
                            if (eyecount(youmonst.data) == 1)
                                strcpy(your_eyes, body_part(EYE));
                            else
                                strcpy(your_eyes, makeplural(body_part(EYE)));
                            pline("%s disintegrates.", name);
                            pline("%s%s body reintegrates before your %s!", name, possessive_suffix(name), your_eyes);
                            pline("%s resurrects!", name);
                        }
                        mon->mhp = mon->mhpmax;
                        break; /* Out of while loop */
                    }
                    if (mon->data == &mons[PM_DEATH] && abstype == ZT_DEATH) {
                        if (canseemon(mon)) {
                            hit(fltxt, mon, ".");
                            char name[BUFSZ];
                            Monnam(name, BUFSZ, mon);
                            pline("%s absorbs the deadly %s!", name, type == ZT_BREATH(ZT_DEATH) ? "blast" : "ray");
                            pline("It seems even stronger than before.");
                        }
                        break; /* Out of while loop */
                    }

                    if (tmp == MAGIC_COOKIE) { /* disintegration */
                        struct obj *otmp2, *m_amulet = mlifesaver(mon);

                        if (canseemon(mon)) {
                            if (!m_amulet) {
                                char name[BUFSZ];
                                Monnam(name, BUFSZ, mon);
                                pline("%s is disintegrated!", name);
                            } else {
                                hit(fltxt, mon, "!");
                            }
                        }
                        mon->mgold = 0L;

                        for (otmp = mon->minvent; otmp; otmp = otmp2) {
                            otmp2 = otmp->nobj;
                            /* note: worn amulet of life saving must be preserved in order to operate */
                            if (!(objects[otmp->otyp].oc_oprop == DISINT_RES || obj_resists(otmp, 5, 50) || is_quest_artifact(otmp) || otmp == m_amulet)) {
                                obj_extract_self(otmp);
                                obfree(otmp, (struct obj *)0);
                            }
                        }

                        if (type < 0)
                            monkilled(mon, (char *)0, -AD_RBRE);
                        else
                            xkilled(mon, 2);
                    } else if (mon->mhp < 1) {
                        if (type < 0)
                            monkilled(mon, fltxt, AD_RBRE);
                        else
                            killed(mon);
                    } else {
                        if (!otmp) {
                            /* normal non-fatal hit */
                            hit(fltxt, mon, exclam(tmp));
                        } else {
                            /* some armor was destroyed; no damage done */
                            if (canseemon(mon)) {
                                char name[BUFSZ];
                                Monnam(name, BUFSZ, mon);
                                pline("%s%s %s is disintegrated!", name, possessive_suffix(name), distant_name(otmp, xname));
                            }
                            m_useup(mon, otmp);
                        }
                        if (mon_could_move && !mon->mcanmove) /* ZT_SLEEP */
                            slept_monst(mon);
                    }
                }
                range -= 2;
            } else {
                miss(fltxt, mon);
            }
        } else if (sx == u.ux && sy == u.uy && range >= 0) {
            nomul(0);
            if (u.usteed && !rn2(3) && !mon_reflects(u.usteed, (char *)0)) {
                mon = u.usteed;
                goto buzzmonst;
            } else if (zap_hit((int)u.uac, 0)) {
                range -= 2;
                pline("%s hits you!", The(fltxt));
                if (Reflecting) {
                    if (!Blind) {
                        (void)ureflects("But %s reflects from your %s!", "it");
                    } else
                        pline("For some reason you are not affected.");
                    dx = -dx;
                    dy = -dy;
                    shieldeff(sx, sy);
                } else {
                    zhitu(type, nd, fltxt, sx, sy);
                }
            } else {
                pline("%s whizzes by you!", The(fltxt));
            }
            if (abstype == ZT_LIGHTNING && !resists_blnd(&youmonst)) {
                You(are_blinded_by_the_flash);
                make_blinded((long)d(nd, 50), false);
                if (!Blind)
                    Your("%s", vision_clears);
            }
            stop_occupation();
            nomul(0);
        }

        if (!ZAP_POS(lev->typ) || (closed_door(sx, sy) && (range >= 0))) {
            int bounce;
            unsigned char rmn;

            make_bounce: if (type == ZT_SPELL(ZT_FIRE)) {
                sx = lsx;
                sy = lsy;
                break; /* fireballs explode before the wall */
            }
            bounce = 0;
            range--;
            if (range && isok(lsx, lsy) && cansee(lsx, lsy))
                pline("%s bounces!", The(fltxt));
            if (!dx || !dy || !rn2(20)) {
                dx = -dx;
                dy = -dy;
            } else {
                if (isok(sx, lsy) && ZAP_POS(rmn = levl[sx][lsy].typ) && !closed_door(sx, lsy) && (IS_ROOM(rmn) || (isok(sx + dx, lsy) && ZAP_POS(levl[sx+dx][lsy].typ))))
                    bounce = 1;
                if (isok(lsx, sy) && ZAP_POS(rmn = levl[lsx][sy].typ) && !closed_door(lsx, sy) && (IS_ROOM(rmn) || (isok(lsx, sy + dy) && ZAP_POS(levl[lsx][sy+dy].typ))))
                    if (!bounce || rn2(2))
                        bounce = 2;

                switch (bounce) {
                    case 0:
                        dx = -dx; /* fall into... */
                    case 1:
                        dy = -dy;
                        break;
                    case 2:
                        dx = -dx;
                        break;
                }
                tmp_at(DISP_CHANGE, zapdir_to_glyph(dx, dy, abstype));
            }
        }
    }
    tmp_at(DISP_END, 0);
    if (type == ZT_SPELL(ZT_FIRE))
        explode(sx, sy, type, d(12, 6), 0, EXPL_FIERY);
    if (shopdamage)
        pay_for_damage(abstype == ZT_FIRE ? "burn away" : abstype == ZT_COLD ? "shatter" : abstype == ZT_DEATH ? "disintegrate" : "destroy", false);
    bhitpos = save_bhitpos;
}

void melt_ice(signed char x, signed char y) {
    struct rm *lev = &levl[x][y];
    struct obj *otmp;

    if (lev->typ == DRAWBRIDGE_UP)
        lev->drawbridgemask &= ~DB_ICE; /* revert to DB_MOAT */
    else { /* lev->typ == ICE */
        lev->typ = (lev->icedpool == ICED_POOL ? POOL : MOAT);
        lev->icedpool = 0;
    }
    obj_ice_effects(x, y, false);
    unearth_objs(x, y);
    if (Underwater)
        vision_recalc(1);
    newsym(x, y);
    if (cansee(x, y))
        Norep("The ice crackles and melts.");
    if ((otmp = sobj_at(BOULDER, x, y)) != 0) {
        if (cansee(x, y))
            pline("%s settles...", An(xname(otmp)));
        do {
            obj_extract_self(otmp); /* boulder isn't being pushed */
            if (!boulder_hits_pool(otmp, x, y, false))
                impossible("melt_ice: no pool?");
            /* try again if there's another boulder and pool didn't fill */
        } while (is_pool(x, y) && (otmp = sobj_at(BOULDER, x, y)) != 0);
        newsym(x, y);
    }
    if (x == u.ux && y == u.uy)
        spoteffects(true); /* possibly drown, notice objects */
}

/* Burn floor scrolls, evaporate pools, etc...  in a single square.  Used
 * both for normal bolts of fire, cold, etc... and for fireballs.
 * Sets shopdamage to true if a shop door is destroyed, and returns the
 * amount by which range is reduced (the latter is just ignored by fireballs)
 */
int zap_over_floor(signed char x, signed char y, int type, bool *shopdamage) {
    struct monst *mon;
    int abstype = abs(type) % 10;
    struct rm *lev = &levl[x][y];
    int rangemod = 0;

    if (abstype == ZT_FIRE) {
        struct trap *t = t_at(x, y);

        if (t && t->ttyp == WEB) {
            /* a burning web is too flimsy to notice if you can't see it */
            if (cansee(x, y))
                Norep("A web bursts into flames!");
            (void)delfloortrap(t);
            if (cansee(x, y))
                newsym(x, y);
        }
        if (is_ice(x, y)) {
            melt_ice(x, y);
        } else if (is_pool(x, y)) {
            const char *msgtxt = "You hear hissing gas.";
            if (lev->typ != POOL) { /* MOAT or DRAWBRIDGE_UP */
                if (cansee(x, y))
                    msgtxt = "Some water evaporates.";
            } else {
                struct trap *ttmp;

                rangemod -= 3;
                lev->typ = ROOM;
                ttmp = maketrap(x, y, PIT);
                if (ttmp)
                    ttmp->tseen = 1;
                if (cansee(x, y))
                    msgtxt = "The water evaporates.";
            }
            Norep("%s", msgtxt);
            if (lev->typ == ROOM)
                newsym(x, y);
        } else if (IS_FOUNTAIN(lev->typ)) {
            if (cansee(x, y))
                pline("Steam billows from the fountain.");
            rangemod -= 1;
            dryup(x, y, type > 0);
        }
    } else if (abstype == ZT_COLD && (is_pool(x, y) || is_lava(x, y))) {
        bool lava = is_lava(x, y);
        bool moat = (!lava && (lev->typ != POOL) && (lev->typ != WATER) && !Is_medusa_level(&u.uz) && !Is_waterlevel(&u.uz));

        if (lev->typ == WATER) {
            /* For now, don't let WATER freeze. */
            if (cansee(x, y))
                pline_The("water freezes for a moment.");
            else
                You_hear("a soft crackling.");
            rangemod -= 1000; /* stop */
        } else {
            rangemod -= 3;
            if (lev->typ == DRAWBRIDGE_UP) {
                lev->drawbridgemask &= ~DB_UNDER; /* clear lava */
                lev->drawbridgemask |= (lava ? DB_FLOOR : DB_ICE);
            } else {
                if (!lava)
                    lev->icedpool = (lev->typ == POOL ? ICED_POOL : ICED_MOAT);
                lev->typ = (lava ? ROOM : ICE);
            }
            bury_objs(x, y);
            if (cansee(x, y)) {
                if (moat)
                    Norep("The moat is bridged with ice!");
                else if (lava)
                    Norep("The lava cools and solidifies.");
                else
                    Norep("The water freezes.");
                newsym(x, y);
            } else if (flags.soundok && !lava)
                You_hear("a crackling sound.");

            if (x == u.ux && y == u.uy) {
                if (u.uinwater) { /* not just `if (Underwater)' */
                    /* leave the no longer existent water */
                    u.uinwater = 0;
                    u.uundetected = 0;
                    docrt();
                    vision_full_recalc = 1;
                } else if (u.utrap && u.utraptype == TT_LAVA) {
                    if (Passes_walls) {
                        You("pass through the now-solid rock.");
                    } else {
                        u.utrap = rn1(50, 20);
                        u.utraptype = TT_INFLOOR;
                        You("are firmly stuck in the cooling rock.");
                    }
                }
            } else if ((mon = m_at(x, y)) != 0) {
                /* probably ought to do some hefty damage to any
                 non-ice creature caught in freezing water;
                 at a minimum, eels are forced out of hiding */
                if (is_swimmer(mon->data) && mon->mundetected) {
                    mon->mundetected = 0;
                    newsym(x, y);
                }
            }
        }
        obj_ice_effects(x, y, true);
    }
    if (closed_door(x, y)) {
        int new_doormask = -1;
        const char *see_txt = 0, *sense_txt = 0, *hear_txt = 0;
        rangemod = -1000;
        switch (abstype) {
            case ZT_FIRE:
                new_doormask = D_NODOOR;
                see_txt = "The door is consumed in flames!";
                sense_txt = "smell smoke.";
                break;
            case ZT_COLD:
                new_doormask = D_NODOOR;
                see_txt = "The door freezes and shatters!";
                sense_txt = "feel cold.";
                break;
            case ZT_DEATH:
                /* death spells/wands don't disintegrate */
                if (abs(type) != ZT_BREATH(ZT_DEATH))
                    goto def_case;
                new_doormask = D_NODOOR;
                see_txt = "The door disintegrates!";
                hear_txt = "crashing wood.";
                break;
            case ZT_LIGHTNING:
                new_doormask = D_BROKEN;
                see_txt = "The door splinters!";
                hear_txt = "crackling.";
                break;
            default:
                def_case: if (cansee(x, y)) {
                    pline_The("door absorbs %s %s!", (type < 0) ? "the" : "your", abs(type) < ZT_SPELL(0) ? "bolt" : abs(type) < ZT_BREATH(0) ? "spell" : "blast");
                } else
                    You_feel("vibrations.");
                break;
        }
        if (new_doormask >= 0) { /* door gets broken */
            if (*in_rooms(x, y, SHOPBASE)) {
                if (type >= 0) {
                    add_damage(x, y, 400L);
                    *shopdamage = true;
                } else
                    /* caused by monster */
                    add_damage(x, y, 0L);
            }
            lev->doormask = new_doormask;
            unblock_point(x, y); /* vision */
            if (cansee(x, y)) {
                plines(see_txt);
                newsym(x, y);
            } else if (sense_txt) {
                You("%s", sense_txt);
            } else if (hear_txt) {
                if (flags.soundok)
                    You_hear("%s", hear_txt);
            }
            if (picking_at(x, y)) {
                stop_occupation();
                reset_pick();
            }
        }
    }

    if (OBJ_AT(x, y) && abstype == ZT_FIRE) {
        if (burn_floor_paper(x, y, false, type > 0) && couldsee(x, y)) {
            newsym(x, y);
            You("%s of smoke.", !Blind ? "see a puff" : "smell a whiff");
        }
    }
    if ((mon = m_at(x, y)) != 0) {
        /* Cannot use wakeup() which also angers the monster */
        mon->msleeping = 0;
        if (mon->m_ap_type)
            seemimic(mon);
        if (type >= 0) {
            setmangry(mon);
            if (mon->ispriest && *in_rooms(mon->mx, mon->my, TEMPLE))
                ghod_hitsu(mon);
            if (mon->isshk && !*u.ushops)
                hot_pursuit(mon);
        }
    }
    return rangemod;
}


/* fractured by pick-axe or wand of striking */
/* no texts here! */
void fracture_rock(struct obj *obj) {
    /* A little Sokoban guilt... */
    if (obj->otyp == BOULDER && In_sokoban(&u.uz) && !flags.mon_moving)
        change_luck(-1);

    obj->otyp = ROCK;
    obj->quan = (long)rn1(60, 7);
    obj->owt = weight(obj);
    obj->oclass = GEM_CLASS;
    obj->known = false;
    obj->onamelth = 0; /* no names */
    obj->oxlth = 0; /* no extra data */
    obj->oattached = OATTACHED_NOTHING;
    if (obj->where == OBJ_FLOOR) {
        obj_extract_self(obj); /* move rocks back on top */
        place_object(obj, obj->ox, obj->oy);
        if (!does_block(obj->ox, obj->oy, &levl[obj->ox][obj->oy]))
            unblock_point(obj->ox, obj->oy);
        if (cansee(obj->ox, obj->oy))
            newsym(obj->ox, obj->oy);
    }
}

/* handle statue hit by striking/force bolt/pick-axe */
bool break_statue(struct obj *obj) {
    /* [obj is assumed to be on floor, so no get_obj_location() needed] */
    struct trap *trap = t_at(obj->ox, obj->oy);
    struct obj *item;

    if (trap && trap->ttyp == STATUE_TRAP && activate_statue_trap(trap, obj->ox, obj->oy, true))
        return false;
    /* drop any objects contained inside the statue */
    while ((item = obj->cobj) != 0) {
        obj_extract_self(item);
        place_object(item, obj->ox, obj->oy);
    }
    if (Role_if(PM_ARCHEOLOGIST) && !flags.mon_moving && (obj->spe & STATUE_HISTORIC)) {
        You_feel("guilty about damaging such a historic statue.");
        adjalign(-1);
    }
    obj->spe = 0;
    fracture_rock(obj);
    return true;
}

/* also used in trap.c */
const char * const destroy_strings[] = {
        "freezes and shatters", "freeze and shatter", "shattered potion",
        "boils and explodes", "boil and explode", "boiling potion",
        "catches fire and burns", "catch fire and burn", "burning scroll",
        "catches fire and burns", "catch fire and burn", "burning book",
        "turns to dust and vanishes", "turn to dust and vanish", "",
        "breaks apart and explodes", "break apart and explode", "exploding wand"
};

void destroy_item(int osym, int dmgtyp) {
    struct obj *obj, *obj2;
    int dmg, xresist, skip;
    long i, cnt, quan;
    int dindx;
    const char *mult;

    for (obj = invent; obj; obj = obj2) {
        obj2 = obj->nobj;
        if (obj->oclass != osym)
            continue; /* test only objs of type osym */
        if (obj->oartifact)
            continue; /* don't destroy artifacts */
        if (obj->in_use && obj->quan == 1)
            continue; /* not available */
        xresist = skip = 0;
        switch (dmgtyp) {
            case AD_COLD:
                if (osym == POTION_CLASS && obj->otyp != POT_OIL) {
                    quan = obj->quan;
                    dindx = 0;
                    dmg = rnd(4);
                } else
                    skip++;
                break;
            case AD_FIRE:
                xresist = (Fire_resistance && obj->oclass != POTION_CLASS);

                if (obj->otyp == SCR_FIRE || obj->otyp == SPE_FIREBALL)
                    skip++;
                if (obj->otyp == SPE_BOOK_OF_THE_DEAD) {
                    skip++;
                    if (!Blind)
                        pline("%s glows a strange %s, but remains intact.", The(xname(obj)), hcolor("dark red"));
                }
                quan = obj->quan;
                switch (osym) {
                    case POTION_CLASS:
                        dindx = 1;
                        dmg = rnd(6);
                        break;
                    case SCROLL_CLASS:
                        dindx = 2;
                        dmg = 1;
                        break;
                    case SPBOOK_CLASS:
                        dindx = 3;
                        dmg = 1;
                        break;
                    default:
                        skip++;
                        break;
                }
                break;
            case AD_ELEC:
                xresist = (Shock_resistance && obj->oclass != RING_CLASS);
                quan = obj->quan;
                switch (osym) {
                    case RING_CLASS:
                        if (obj->otyp == RIN_SHOCK_RESISTANCE) {
                            skip++;
                            break;
                        }
                        dindx = 4;
                        dmg = 0;
                        break;
                    case WAND_CLASS:
                        if (obj->otyp == WAN_LIGHTNING) {
                            skip++;
                            break;
                        }
                        dindx = 5;
                        dmg = rnd(10);
                        break;
                    default:
                        skip++;
                        break;
                }
                break;
            default:
                skip++;
                break;
        }
        if (!skip) {
            if (obj->in_use)
                --quan; /* one will be used up elsewhere */
            for (i = cnt = 0L; i < quan; i++)
                if (!rn2(3))
                    cnt++;

            if (!cnt)
                continue;
            if (cnt == quan)
                mult = "Your";
            else
                mult = (cnt == 1L) ? "One of your" : "Some of your";
            pline("%s %s %s!", mult, xname(obj), (cnt > 1L) ? destroy_strings[dindx * 3 + 1] : destroy_strings[dindx * 3]);
            if (osym == POTION_CLASS && dmgtyp != AD_COLD) {
                if (!breathless(youmonst.data) || haseyes(youmonst.data))
                    potionbreathe(obj);
            }
            if (obj->owornmask) {
                if (obj->owornmask & W_RING) /* ring being worn */
                    Ring_gone(obj);
                else
                    setnotworn(obj);
            }
            if (obj == current_wand)
                current_wand = 0; /* destroyed */
            for (i = 0; i < cnt; i++)
                useup(obj);
            if (dmg) {
                if (xresist)
                    You("aren't hurt!");
                else {
                    const char *how = destroy_strings[dindx * 3 + 2];
                    bool one = (cnt == 1L);

                    losehp(dmg, one ? how : (const char *)makeplural(how), one ? KILLED_BY_AN : KILLED_BY);
                    exercise(A_STR, false);
                }
            }
        }
    }
}

int destroy_mitem(struct monst *mtmp, int osym, int dmgtyp) {
    struct obj *obj, *obj2;
    int skip, tmp = 0;
    long i, cnt, quan;
    int dindx;
    bool vis;

    if (mtmp == &youmonst) { /* this simplifies artifact_hit() */
        destroy_item(osym, dmgtyp);
        return 0; /* arbitrary; value doesn't matter to artifact_hit() */
    }

    vis = canseemon(mtmp);
    for (obj = mtmp->minvent; obj; obj = obj2) {
        obj2 = obj->nobj;
        if (obj->oclass != osym)
            continue; /* test only objs of type osym */
        skip = 0;
        quan = 0L;
        dindx = 0;

        switch (dmgtyp) {
            case AD_COLD:
                if (osym == POTION_CLASS && obj->otyp != POT_OIL) {
                    quan = obj->quan;
                    dindx = 0;
                    tmp++;
                } else
                    skip++;
                break;
            case AD_FIRE:
                if (obj->otyp == SCR_FIRE || obj->otyp == SPE_FIREBALL)
                    skip++;
                if (obj->otyp == SPE_BOOK_OF_THE_DEAD) {
                    skip++;
                    if (vis)
                        pline("%s glows a strange %s, but remains intact.", The(distant_name(obj, xname)), hcolor("dark red"));
                }
                quan = obj->quan;
                switch (osym) {
                    case POTION_CLASS:
                        dindx = 1;
                        tmp++;
                        break;
                    case SCROLL_CLASS:
                        dindx = 2;
                        tmp++;
                        break;
                    case SPBOOK_CLASS:
                        dindx = 3;
                        tmp++;
                        break;
                    default:
                        skip++;
                        break;
                }
                break;
            case AD_ELEC:
                quan = obj->quan;
                switch (osym) {
                    case RING_CLASS:
                        if (obj->otyp == RIN_SHOCK_RESISTANCE) {
                            skip++;
                            break;
                        }
                        dindx = 4;
                        break;
                    case WAND_CLASS:
                        if (obj->otyp == WAN_LIGHTNING) {
                            skip++;
                            break;
                        }
                        dindx = 5;
                        tmp++;
                        break;
                    default:
                        skip++;
                        break;
                }
                break;
            default:
                skip++;
                break;
        }
        if (!skip) {
            for (i = cnt = 0L; i < quan; i++)
                if (!rn2(3))
                    cnt++;

            if (!cnt)
                continue;
            if (vis) {
                char name[BUFSZ];
                Monnam(name, BUFSZ, mtmp);
                const char * destroyed_message = (cnt > 1L) ? destroy_strings[dindx * 3 + 1] : destroy_strings[dindx * 3];
                pline("%s%s %s %s!", name, possessive_suffix(name), xname(obj), destroyed_message);
            }
            for (i = 0; i < cnt; i++)
                m_useup(mtmp, obj);
        }
    }
    return (tmp);
}


int resist(struct monst *mtmp, char oclass, int damage, int tell) {
    int resisted;
    int alev, dlev;

    /* attack level */
    switch (oclass) {
        case WAND_CLASS:
            alev = 12;
            break;
        case TOOL_CLASS:
            alev = 10;
            break; /* instrument */
        case WEAPON_CLASS:
            alev = 10;
            break; /* artifact */
        case SCROLL_CLASS:
            alev = 9;
            break;
        case POTION_CLASS:
            alev = 6;
            break;
        case RING_CLASS:
            alev = 5;
            break;
        default:
            alev = u.ulevel;
            break; /* spell */
    }
    /* defense level */
    dlev = (int)mtmp->m_lev;
    if (dlev > 50)
        dlev = 50;
    else if (dlev < 1)
        dlev = is_mplayer(mtmp->data) ? u.ulevel : 1;

    resisted = rn2(100 + alev - dlev) < mtmp->data->mr;
    if (resisted) {
        if (tell) {
            shieldeff(mtmp->mx, mtmp->my);
            char name[BUFSZ];
            Monnam(name, BUFSZ, mtmp);
            pline("%s resists!", name);
        }
        damage = (damage + 1) / 2;
    }

    if (damage) {
        mtmp->mhp -= damage;
        if (mtmp->mhp < 1) {
            if (m_using)
                monkilled(mtmp, "", AD_RBRE);
            else
                killed(mtmp);
        }
    }
    return (resisted);
}

void makewish(void) {
    char buf[BUFSZ];
    struct obj *otmp, nothing;
    int tries = 0;

    nothing = zeroobj; /* lint suppression; only its address matters */
    if (flags.verbose)
        You("may wish for an object.");
    retry: getlin("For what do you wish?", buf);
    if (buf[0] == '\033')
        buf[0] = 0;
    /*
     *  Note: if they wished for and got a non-object successfully,
     *  otmp == &zeroobj.  That includes gold, or an artifact that
     *  has been denied.  Wishing for "nothing" requires a separate
     *  value to remain distinct.
     */
    otmp = readobjnam(buf, &nothing, true);
    if (!otmp) {
        pline("Nothing fitting that description exists in the game.");
        if (++tries < 5)
            goto retry;
        plines(thats_enough_tries);
        otmp = readobjnam((char *)0, (struct obj *)0, true);
        if (!otmp)
            return; /* for safety; should never happen */
    } else if (otmp == &nothing) {
        /* explicitly wished for "nothing", presumeably attempting
         to retain wishless conduct */
        return;
    }

    /* KMH, conduct */
    u.uconduct.wishes++;

    if (otmp != &zeroobj) {
        /* The(aobjnam()) is safe since otmp is unidentified -dlc */
        (void)hold_another_object(otmp, u.uswallow ? "Oops!  %s out of your reach!" : (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz) ||
                levl[u.ux][u.uy].typ < IRONBARS ||
                levl[u.ux][u.uy].typ >= ICE) ? "Oops!  %s away from you!" : "Oops!  %s to the floor!", The(aobjnam(otmp,
                        Is_airlevel(&u.uz) || u.uinwater ? "slip" : "drop")), (const char *)0);
        u.ublesscnt += rn1(100, 50); /* the gods take notice */
    }
}
