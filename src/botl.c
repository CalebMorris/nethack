/* See LICENSE in the root of this project for change info */

#include "hack.h"
#include "wintty.h"
#include "pm_props.h"
#include "extern.h"
#include "winprocs.h"
#include "color.h"
#include "youprop.h"
#include "flag.h"

extern const char *hu_stat[];   /* defined in eat.c */

const char * const enc_stat[] = {
    "",
    "Burdened",
    "Stressed",
    "Strained",
    "Overtaxed",
    "Overloaded"
};

static void bot1(void);
static void bot2(void);

/* MAXCO must hold longest uncompressed status line, and must be larger
 * than COLNO
 *
 * longest practical second status line at the moment is
 *      Astral Plane $:12345 HP:700(700) Pw:111(111) AC:-127 Xp:30/123456789
 *      T:123456 Satiated Conf FoodPois Ill Blind Stun Hallu Overloaded
 * -- or somewhat over 130 characters
 */
#define MAXCO 160

static int mrank_sz = 0; /* loaded by max_rank_sz (from u_init) */
static const char *rank(void);

/* convert experience level (1..30) to rank index (0..8) */
int xlev_to_rank (int xlev) {
    return (xlev <= 2) ? 0 : (xlev <= 30) ? ((xlev + 2) / 4) : 8;
}

const char * rank_of(int lev, short monnum, bool female) {
    struct Role *role;
    int i;


    /* Find the role */
    for (role = (struct Role *) roles; role->name.m; role++)
        if (monnum == role->malenum || monnum == role->femalenum)
            break;
    if (!role->name.m)
        role = &urole;

    /* Find the rank */
    for (i = xlev_to_rank((int)lev); i >= 0; i--) {
        if (female && role->rank[i].f) return (role->rank[i].f);
        if (role->rank[i].m) return (role->rank[i].m);
    }

    /* Try the role name, instead */
    if (female && role->name.f) return (role->name.f);
    else if (role->name.m) return (role->name.m);
    return ("Player");
}

static const char * rank (void) {
    return(rank_of(u.ulevel, Role_switch, flags.female));
}

int title_to_mon (const char *str, int *rank_indx, int *title_length) {
    int i, j;


    /* Loop through each of the roles */
    for (i = 0; roles[i].name.m; i++)
        for (j = 0; j < 9; j++) {
            if (roles[i].rank[j].m && !strncmpi(str,
                        roles[i].rank[j].m, strlen(roles[i].rank[j].m))) {
                if (rank_indx) *rank_indx = j;
                if (title_length) *title_length = strlen(roles[i].rank[j].m);
                return roles[i].malenum;
            }
            if (roles[i].rank[j].f && !strncmpi(str,
                        roles[i].rank[j].f, strlen(roles[i].rank[j].f))) {
                if (rank_indx) *rank_indx = j;
                if (title_length) *title_length = strlen(roles[i].rank[j].f);
                return ((roles[i].femalenum != NON_PM) ?
                        roles[i].femalenum : roles[i].malenum);
            }
        }
    return NON_PM;
}

void max_rank_sz (void) {
    int i, r, maxr = 0;
    for (i = 0; i < 9; i++) {
        if (urole.rank[i].m && (r = strlen(urole.rank[i].m)) > maxr) maxr = r;
        if (urole.rank[i].f && (r = strlen(urole.rank[i].f)) > maxr) maxr = r;
    }
    mrank_sz = maxr;
    return;
}

void bot1str(char *newbot1) {
    char *nb;
    int i,j;

    strcpy(newbot1, plname);
    if('a' <= newbot1[0] && newbot1[0] <= 'z') newbot1[0] += 'A'-'a';
    newbot1[10] = 0;
    sprintf(nb = eos(newbot1)," the ");

    if (Upolyd) {
        char mbot[BUFSZ];
        int k = 0;

        strcpy(mbot, mons[u.umonnum].mname);
        while(mbot[k] != 0) {
            if ((k == 0 || (k > 0 && mbot[k-1] == ' ')) &&
                    'a' <= mbot[k] && mbot[k] <= 'z')
                mbot[k] += 'A' - 'a';
            k++;
        }
        sprintf(nb = eos(nb), "%s", mbot);
    } else
        sprintf(nb = eos(nb), "%s", rank());

    sprintf(nb = eos(nb),"  ");
    i = mrank_sz + 15;
    j = (nb + 2) - newbot1; /* aka strlen(newbot1) but less computation */
    if((i - j) > 0)
        sprintf(nb = eos(nb),"%*s", i-j, " ");  /* pad with spaces */
    if (ACURR(A_STR) > 18) {
        if (ACURR(A_STR) > STR18(100))
            sprintf(nb = eos(nb),"St:%2d ",ACURR(A_STR)-100);
        else if (ACURR(A_STR) < STR18(100))
            sprintf(nb = eos(nb), "St:18/%02d ",ACURR(A_STR)-18);
        else
            sprintf(nb = eos(nb),"St:18/** ");
    } else
        sprintf(nb = eos(nb), "St:%-1d ",ACURR(A_STR));
    sprintf(nb = eos(nb),
            "Dx:%-1d Co:%-1d In:%-1d Wi:%-1d Ch:%-1d",
            ACURR(A_DEX), ACURR(A_CON), ACURR(A_INT), ACURR(A_WIS), ACURR(A_CHA));
    sprintf(nb = eos(nb), (u.ualign.type == A_CHAOTIC) ? "  Chaotic" :
            (u.ualign.type == A_NEUTRAL) ? "  Neutral" : "  Lawful");
}

static void bot1 (void) {
    char newbot1[MAXCO];

    bot1str(newbot1);
    curs(WIN_STATUS, 1, 0);
    putstr(WIN_STATUS, 0, newbot1);
}

/* provide the name of the current level for display by various ports */
int describe_level (char *buf) {
    int ret = 1;

    /* TODO:        Add in dungeon name */
    if (Is_knox(&u.uz))
        sprintf(buf, "%s ", dungeons[u.uz.dnum].dname);
    else if (In_quest(&u.uz))
        sprintf(buf, "Home %d ", dunlev(&u.uz));
    else if (In_endgame(&u.uz))
        sprintf(buf,
                Is_astralevel(&u.uz) ? "Astral Plane " : "End Game ");
    else {
        /* ports with more room may expand this one */
        sprintf(buf, "Dlvl:%-2d ", depth(&u.uz));
        ret = 0;
    }
    return ret;
}

void bot2str (char *newbot2) {
    char *nb;
    int hp, hpmax;
    int hpcolor, hpattr;
    int cap = near_capacity();

    hp = Upolyd ? u.mh : u.uhp;
    hpmax = Upolyd ? u.mhmax : u.uhpmax;

    if(hp < 0) hp = 0;
    (void) describe_level(newbot2);
    sprintf(nb = eos(newbot2),
            "%c:%-2ld HP:", oc_syms[COIN_CLASS],
            u.ugold
           );
    curs(WIN_STATUS, 1, 1);
    putstr(WIN_STATUS, 0, newbot2);

    sprintf(nb = eos(newbot2), "%d(%d)", hp, hpmax);
    if (iflags.use_color && iflags.use_hpmon) {
        curs(WIN_STATUS, 1, 1);
        hpattr = ATR_NONE;
        if(hp == hpmax){
            hpcolor = NO_COLOR;
        } else if(hp > (hpmax*2/3)) {
            hpcolor = CLR_GREEN;
        } else if(hp <= (hpmax/3)) {
            hpcolor = CLR_RED;
            if(hp<=(hpmax/10))
                hpattr = ATR_BLINK;
        } else {
            hpcolor = CLR_YELLOW;
        }
        if (hpcolor != NO_COLOR)
            term_start_color(hpcolor);
        if(hpattr!=ATR_NONE)term_start_attr(hpattr);
        putstr(WIN_STATUS, hpattr, newbot2);
        if(hpattr!=ATR_NONE)term_end_attr(hpattr);
        if (hpcolor != NO_COLOR)
            term_end_color();
    }
    sprintf(nb = eos(newbot2), " Pw:%d(%d) AC:%-2d",
            u.uen, u.uenmax, u.uac);

    if (Upolyd)
        sprintf(nb = eos(nb), " HD:%d", mons[u.umonnum].mlevel);
    else if(flags.showexp)
        sprintf(nb = eos(nb), " Xp:%u/%-1ld", u.ulevel,u.uexp);
    else
        sprintf(nb = eos(nb), " Exp:%u", u.ulevel);

    if(flags.time)
        sprintf(nb = eos(nb), " T:%ld", moves);
    if(strcmp(hu_stat[u.uhs], "        ")) {
        sprintf(nb = eos(nb), " ");
        strcat(newbot2, hu_stat[u.uhs]);
    }
    if(Confusion)      sprintf(nb = eos(nb), " Conf");
    if(Sick) {
        if (u.usick_type & SICK_VOMITABLE)
            sprintf(nb = eos(nb), " FoodPois");
        if (u.usick_type & SICK_NONVOMITABLE)
            sprintf(nb = eos(nb), " Ill");
    }
    if(Blind)          sprintf(nb = eos(nb), " Blind");
    if(Stunned)        sprintf(nb = eos(nb), " Stun");
    if(Hallucination)  sprintf(nb = eos(nb), " Hallu");
    if(Slimed)         sprintf(nb = eos(nb), " Slime");
    if(cap > UNENCUMBERED)
        sprintf(nb = eos(nb), " %s", enc_stat[cap]);
}

static void bot2 (void) {
    char newbot2[MAXCO];
    bot2str(newbot2);
    curs(WIN_STATUS, 1, 1);
    putstr(WIN_STATUS, 0, newbot2);
}

void bot (void) {
    bot1();
    bot2();
    flags.botl = flags.botlx = 0;
}
