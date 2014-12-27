/* See LICENSE in the root of this project for change info */
#include <stdarg.h>
#include "hack.h"
#include "epri.h"
#include "edog.h"
#include "extern.h"
#include "winprocs.h"
#include "youprop.h"
#include "flag.h"

static bool no_repeat = false;

static char *You_buf(int);


static void vpline(const char *, va_list);

void pline(const char * line, ...) {
    va_list the_args;
        va_start(the_args, line);
        vpline(line, the_args);
    va_end(the_args);
}

static void vpline(const char *line, va_list the_args) {
        char pbuf[BUFSZ];
/* Do NOT use va_start and va_start in here... see above */

        if (!line || !*line) return;
        if (index(line, '%')) {
            Vsprintf(pbuf,line,the_args);
            line = pbuf;
        }
        plines(line);
}

void plines(const char *line) {
        if (!line || !*line) return;
        if (!iflags.window_inited) {
            raw_print(line);
            return;
        }
        if (no_repeat && !strcmp(line, toplines))
            return;
        if (vision_full_recalc) vision_recalc(0);
        if (u.ux) flush_screen(1);              /* %% */
        putstr(WIN_MESSAGE, 0, line);
}

void Norep (const char * line, ...) {
    va_list the_args;
        va_start(the_args, line);
        no_repeat = true;
        vpline(line, the_args);
        no_repeat = false;
    va_end(the_args);
        return;
}

/* work buffer for You(), &c and verbalize() */
static char *you_buf = 0;
static int you_buf_siz = 0;

static char * You_buf (int siz) {
        if (siz > you_buf_siz) {
                if (you_buf) free((void *) you_buf);
                you_buf_siz = siz + 10;
                you_buf = (char *) alloc((unsigned) you_buf_siz);
        }
        return you_buf;
}

void free_youbuf (void) {
        if (you_buf) free((void *) you_buf),  you_buf = (char *)0;
        you_buf_siz = 0;
}

/* `prefix' must be a string literal, not a pointer */
#define YouPrefix(pointer,prefix,text) \
 Strcpy((pointer = You_buf((int)(strlen(text) + sizeof prefix))), prefix)

#define YouMessage(pointer,prefix,text) \
 strcat((YouPrefix(pointer, prefix, text), pointer), text)

void You (const char * line, ...) {
    va_list the_args;
        char *tmp;
        va_start(the_args, line);
        vpline(YouMessage(tmp, "You ", line), the_args);
    va_end(the_args);
}

void Your (const char * line, ...) {
    va_list the_args;
        char *tmp;
        va_start(the_args, line);
        vpline(YouMessage(tmp, "Your ", line), the_args);
    va_end(the_args);
}

void You_feel (const char * line, ...) {
    va_list the_args;
        char *tmp;
        va_start(the_args, line);
        vpline(YouMessage(tmp, "You feel ", line), the_args);
    va_end(the_args);
}

void You_cant (const char * line, ...) {
    va_list the_args;
        char *tmp;
        va_start(the_args, line);
        vpline(YouMessage(tmp, "You can't ", line), the_args);
    va_end(the_args);
}

void pline_The (const char * line, ...) {
    va_list the_args;
        char *tmp;
        va_start(the_args, line);
        vpline(YouMessage(tmp, "The ", line), the_args);
    va_end(the_args);
}

void There (const char * line, ...) {
    va_list the_args;
        char *tmp;
        va_start(the_args, line);
        vpline(YouMessage(tmp, "There ", line), the_args);
    va_end(the_args);
}

void You_hear (const char * line, ...) {
    va_list the_args;
        char *tmp;
        va_start(the_args, line);
        if (Underwater)
                YouPrefix(tmp, "You barely hear ", line);
        else if (u.usleep)
                YouPrefix(tmp, "You dream that you hear ", line);
        else
                YouPrefix(tmp, "You hear ", line);
        vpline(strcat(tmp, line), the_args);
    va_end(the_args);
}

void verbalize (const char *line, ...) {
    va_list the_args;
        char *tmp;
        if (!flags.soundok) return;
        va_start(the_args, line);
        tmp = You_buf((int)strlen(line) + sizeof "\"\"");
        Strcpy(tmp, "\"");
        Strcat(tmp, line);
        Strcat(tmp, "\"");
        vpline(tmp, the_args);
    va_end(the_args);
}

static void vraw_printf(const char *,va_list);

void raw_printf (const char * line, ...) {
    va_list the_args;
        va_start(the_args, line);
        vraw_printf(line, the_args);
    va_end(the_args);
}

static void vraw_printf(const char *line, va_list the_args) {

/* Do NOT use va_start and va_end in here... see above */

        if(!index(line, '%'))
            raw_print(line);
        else {
            char pbuf[BUFSZ];
            Vsprintf(pbuf,line,the_args);
            raw_print(pbuf);
        }
}


void impossible (const char * s, ...) {
    va_list the_args;
        va_start(the_args, s);
        if (program_state.in_impossible)
                panic("impossible called impossible");
        program_state.in_impossible = 1;
        {
            char pbuf[BUFSZ];
            Vsprintf(pbuf,s,the_args);
            paniclog("impossible", pbuf);
        }
        vpline(s,the_args);
        pline("Program in disorder - perhaps you'd better #quit.");
        program_state.in_impossible = 0;
    va_end(the_args);
}

const char * align_str (aligntyp alignment) {
    switch ((int)alignment) {
        case A_CHAOTIC: return "chaotic";
        case A_NEUTRAL: return "neutral";
        case A_LAWFUL:  return "lawful";
        case A_NONE:    return "unaligned";
    }
    return "unknown";
}

void mstatusline (struct monst *mtmp) {
        aligntyp alignment;
        char info[BUFSZ], monnambuf[BUFSZ];

        if (mtmp->ispriest || mtmp->data == &mons[PM_ALIGNED_PRIEST]
                                || mtmp->data == &mons[PM_ANGEL])
                alignment = EPRI(mtmp)->shralign;
        else
                alignment = mtmp->data->maligntyp;
        alignment = (alignment > 0) ? A_LAWFUL :
                (alignment < 0) ? A_CHAOTIC :
                A_NEUTRAL;

        info[0] = 0;
        if (mtmp->mtame) {        Strcat(info, ", tame");
            if (wizard) {
                Sprintf(eos(info), " (%d", mtmp->mtame);
                if (!mtmp->isminion)
                    Sprintf(eos(info), "; hungry %ld; apport %d",
                        EDOG(mtmp)->hungrytime, EDOG(mtmp)->apport);
                Strcat(info, ")");
            }
        }
        else if (mtmp->mpeaceful) Strcat(info, ", peaceful");
        if (mtmp->meating)        Strcat(info, ", eating");
        if (mtmp->mcan)           Strcat(info, ", cancelled");
        if (mtmp->mconf)          Strcat(info, ", confused");
        if (mtmp->mblinded || !mtmp->mcansee)
                                  Strcat(info, ", blind");
        if (mtmp->mstun)          Strcat(info, ", stunned");
        if (mtmp->msleeping)      Strcat(info, ", asleep");
        else if (mtmp->mfrozen || !mtmp->mcanmove)
                                  Strcat(info, ", can't move");
                                  /* [arbitrary reason why it isn't moving] */
        else if (mtmp->mstrategy & STRAT_WAITMASK)
                                  Strcat(info, ", meditating");
        else if (mtmp->mflee)     Strcat(info, ", scared");
        if (mtmp->mtrapped)       Strcat(info, ", trapped");
        if (mtmp->mspeed)         Strcat(info,
                                        mtmp->mspeed == MFAST ? ", fast" :
                                        mtmp->mspeed == MSLOW ? ", slow" :
                                        ", ???? speed");
        if (mtmp->mundetected)    Strcat(info, ", concealed");
        if (mtmp->minvis)         Strcat(info, ", invisible");
        if (mtmp == u.ustuck)     Strcat(info,
                        (sticks(youmonst.data)) ? ", held by you" :
                                u.uswallow ? (is_animal(u.ustuck->data) ?
                                ", swallowed you" :
                                ", engulfed you") :
                                ", holding you");
        if (mtmp == u.usteed)     Strcat(info, ", carrying you");

        /* avoid "Status of the invisible newt ..., invisible" */
        /* and unlike a normal mon_nam, use "saddled" even if it has a name */
        Strcpy(monnambuf, x_monnam(mtmp, ARTICLE_THE, (char *)0,
            (SUPPRESS_IT|SUPPRESS_INVISIBLE), false));

        pline("Status of %s (%s):  Level %d  HP %d(%d)  AC %d%s.",
                monnambuf,
                align_str(alignment),
                mtmp->m_lev,
                mtmp->mhp,
                mtmp->mhpmax,
                find_mac(mtmp),
                info);
}

void
ustatusline (void)
{
        char info[BUFSZ];

        info[0] = '\0';
        if (Sick) {
                Strcat(info, ", dying from");
                if (u.usick_type & SICK_VOMITABLE)
                        Strcat(info, " food poisoning");
                if (u.usick_type & SICK_NONVOMITABLE) {
                        if (u.usick_type & SICK_VOMITABLE)
                                Strcat(info, " and");
                        Strcat(info, " illness");
                }
        }
        if (Stoned)             Strcat(info, ", solidifying");
        if (Slimed)             Strcat(info, ", becoming slimy");
        if (Strangled)          Strcat(info, ", being strangled");
        if (Vomiting)           Strcat(info, ", nauseated"); /* !"nauseous" */
        if (Confusion)          Strcat(info, ", confused");
        if (Blind) {
            Strcat(info, ", blind");
            if (u.ucreamed) {
                if ((long)u.ucreamed < Blinded || Blindfolded
                                                || !haseyes(youmonst.data))
                    Strcat(info, ", cover");
                Strcat(info, "ed by sticky goop");
            }   /* note: "goop" == "glop"; variation is intentional */
        }
        if (Stunned)            Strcat(info, ", stunned");
        if (!u.usteed)
        if (Wounded_legs) {
            const char *what = body_part(LEG);
            if ((Wounded_legs & BOTH_SIDES) == BOTH_SIDES)
                what = makeplural(what);
                                Sprintf(eos(info), ", injured %s", what);
        }
        if (Glib)               Sprintf(eos(info), ", slippery %s",
                                        makeplural(body_part(HAND)));
        if (u.utrap)            Strcat(info, ", trapped");
        if (Fast)               Strcat(info, Very_fast ?
                                                ", very fast" : ", fast");
        if (u.uundetected)      Strcat(info, ", concealed");
        if (Invis)              Strcat(info, ", invisible");
        if (u.ustuck) {
            if (sticks(youmonst.data))
                Strcat(info, ", holding ");
            else
                Strcat(info, ", held by ");
            Strcat(info, mon_nam(u.ustuck));
        }

        pline("Status of %s (%s%s):  Level %d  HP %d(%d)  AC %d%s.",
                plname,
                    (u.ualign.record >= 20) ? "piously " :
                    (u.ualign.record > 13) ? "devoutly " :
                    (u.ualign.record > 8) ? "fervently " :
                    (u.ualign.record > 3) ? "stridently " :
                    (u.ualign.record == 3) ? "" :
                    (u.ualign.record >= 1) ? "haltingly " :
                    (u.ualign.record == 0) ? "nominally " :
                                            "insufficiently ",
                align_str(u.ualign.type),
                Upolyd ? mons[u.umonnum].mlevel : u.ulevel,
                Upolyd ? u.mh : u.uhp,
                Upolyd ? u.mhmax : u.uhpmax,
                u.uac,
                info);
}

void
self_invis_message (void)
{
        pline("%s %s.",
            Hallucination ? "Far out, man!  You" : "Gee!  All of a sudden, you",
            See_invisible ? "can see right through yourself" :
                "can't see yourself");
}

/*pline.c*/
