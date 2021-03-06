/* See LICENSE in the root of this project for change info */
/* various code that was replicated in *main.c */

#include "hack.h"

#include <signal.h>

#include "dlb.h"
#include "pm_props.h"
#include "flag.h"
#include "decl.h"
#include "permonst.h"
#include "rm.h"
#include "youprop.h"
#include "extern.h"
#include "winprocs.h"

#include <sys/stat.h>
#include <signal.h>
#include <pwd.h>
#include <fcntl.h>

static bool whoami(void);
static void process_options(int, char **);

extern void check_linux_console(void);
extern void init_linux_cons(void);

static void wd_message(void);

// the file containing the latest hack news
static const char *NEWS = "news";

void moveloop(void) {
    int moveamt = 0, wtcap = 0, change = 0;
    bool didmove = false, monscanmove = false;

    flags.moonphase = phase_of_the_moon();
    if(flags.moonphase == FULL_MOON) {
        You("are lucky!  Full moon tonight.");
        change_luck(1);
    } else if(flags.moonphase == NEW_MOON) {
        pline("Be careful!  New moon tonight.");
    }
    flags.friday13 = friday_13th();
    if (flags.friday13) {
        pline("Watch out!  Bad things can happen on Friday the 13th.");
        change_luck(-1);
    }

    initrack();


    /* Note:  these initializers don't do anything except guarantee that
       we're linked properly.
       */
    decl_init();
    monst_init();
    monstr_init();      /* monster strengths */
    objects_init();

    if (wizard) add_debug_extended_commands();

    (void) encumber_msg(); /* in case they auto-picked up something */

    u.uz0.dlevel = u.uz.dlevel;
    youmonst.movement = NORMAL_SPEED;   /* give the hero some movement points */

    for(;;) {
        get_nh_event();

        didmove = flags.move;
        if(didmove) {
            /* actual time passed */
            youmonst.movement -= NORMAL_SPEED;

            do { /* hero can't move this turn loop */
                wtcap = encumber_msg();

                flags.mon_moving = true;
                do {
                    monscanmove = movemon();
                    if (youmonst.movement > NORMAL_SPEED)
                        break;  /* it's now your turn */
                } while (monscanmove);
                flags.mon_moving = false;

                if (!monscanmove && youmonst.movement < NORMAL_SPEED) {
                    /* both you and the monsters are out of steam this round */
                    /* set up for a new turn */
                    struct monst *mtmp;
                    mcalcdistress();    /* adjust monsters' trap, blind, etc */

                    /* reallocate movement rations to monsters */
                    for (mtmp = fmon; mtmp; mtmp = mtmp->nmon)
                        mtmp->movement += mcalcmove(mtmp);

                    if(!rn2(u.uevent.udemigod ? 25 :
                                (depth(&u.uz) > depth(&stronghold_level)) ? 50 : 70))
                        (void) makemon((struct permonst *)0, 0, 0, NO_MM_FLAGS);

                    /* calculate how much time passed. */
                    if (u.usteed && u.umoved) {
                        /* your speed doesn't augment steed's speed */
                        moveamt = mcalcmove(u.usteed);
                    } else
                    {
                        moveamt = youmonst.data->mmove;

                        if (Very_fast) {        /* speed boots or potion */
                            /* average movement is 1.67 times normal */
                            moveamt += NORMAL_SPEED / 2;
                            if (rn2(3) == 0) moveamt += NORMAL_SPEED / 2;
                        } else if (Fast) {
                            /* average movement is 1.33 times normal */
                            if (rn2(3) != 0) moveamt += NORMAL_SPEED / 2;
                        }
                    }

                    switch (wtcap) {
                        case UNENCUMBERED: break;
                        case SLT_ENCUMBER: moveamt -= (moveamt / 4); break;
                        case MOD_ENCUMBER: moveamt -= (moveamt / 2); break;
                        case HVY_ENCUMBER: moveamt -= ((moveamt * 3) / 4); break;
                        case EXT_ENCUMBER: moveamt -= ((moveamt * 7) / 8); break;
                        default: break;
                    }

                    youmonst.movement += moveamt;
                    if (youmonst.movement < 0) youmonst.movement = 0;
                    settrack();

                    monstermoves++;
                    moves++;

                    /********************************/
                    /* once-per-turn things go here */
                    /********************************/

                    if (flags.bypasses) clear_bypasses();
                    if(Glib) glibr();
                    nh_timeout();
                    run_regions();

                    if (u.ublesscnt)  u.ublesscnt--;
                    if(flags.time && !flags.run)
                        flags.botl = 1;

                    /* One possible result of prayer is healing.  Whether or
                     * not you get healed depends on your current hit points.
                     * If you are allowed to regenerate during the prayer, the
                     * end-of-prayer calculation messes up on this.
                     * Another possible result is rehumanization, which requires
                     * that encumbrance and movement rate be recalculated.
                     */
                    if (u.uinvulnerable) {
                        /* for the moment at least, you're in tiptop shape */
                        wtcap = UNENCUMBERED;
                    } else if (Upolyd && youmonst.data->mlet == S_EEL && !is_pool(u.ux,u.uy) && !Is_waterlevel(&u.uz)) {
                        if (u.mh > 1) {
                            u.mh--;
                            flags.botl = 1;
                        } else if (u.mh < 1)
                            rehumanize();
                    } else if (Upolyd && u.mh < u.mhmax) {
                        if (u.mh < 1)
                            rehumanize();
                        else if (Regeneration ||
                                (wtcap < MOD_ENCUMBER && !(moves%20))) {
                            flags.botl = 1;
                            u.mh++;
                        }
                    } else if (u.uhp < u.uhpmax &&
                            (wtcap < MOD_ENCUMBER || !u.umoved || Regeneration)) {
                        if (u.ulevel > 9 && !(moves % 3)) {
                            int heal, Con = (int) ACURR(A_CON);

                            if (Con <= 12) {
                                heal = 1;
                            } else {
                                heal = rnd(Con);
                                if (heal > u.ulevel-9) heal = u.ulevel-9;
                            }
                            flags.botl = 1;
                            u.uhp += heal;
                            if(u.uhp > u.uhpmax)
                                u.uhp = u.uhpmax;
                        } else if (Regeneration ||
                                (u.ulevel <= 9 &&
                                 !(moves % ((MAXULEV+12) / (u.ulevel+2) + 1)))) {
                            flags.botl = 1;
                            u.uhp++;
                        }
                    }

                    /* moving around while encumbered is hard work */
                    if (wtcap > MOD_ENCUMBER && u.umoved) {
                        if(!(wtcap < EXT_ENCUMBER ? moves%30 : moves%10)) {
                            if (Upolyd && u.mh > 1) {
                                u.mh--;
                            } else if (!Upolyd && u.uhp > 1) {
                                u.uhp--;
                            } else {
                                You("pass out from exertion!");
                                exercise(A_CON, false);
                                fall_asleep(-10, false);
                            }
                        }
                    }

                    if ((u.uen < u.uenmax) &&
                            ((wtcap < MOD_ENCUMBER &&
                              (!(moves%((MAXULEV + 8 - u.ulevel) *
                                        (Role_if(PM_WIZARD) ? 3 : 4) / 6))))
                             || Energy_regeneration)) {
                        u.uen += rn1((int)(ACURR(A_WIS) + ACURR(A_INT)) / 15 + 1,1);
                        if (u.uen > u.uenmax)  u.uen = u.uenmax;
                        flags.botl = 1;
                    }

                    if(!u.uinvulnerable) {
                        if(Teleportation && !rn2(85)) {
                            signed char old_ux = u.ux, old_uy = u.uy;
                            tele();
                            if (u.ux != old_ux || u.uy != old_uy) {
                                if (!next_to_u()) {
                                    check_leash(old_ux, old_uy);
                                }
                                /* clear doagain keystrokes */
                                pushch(0);
                                savech(0);
                            }
                        }
                        /* delayed change may not be valid anymore */
                        if ((change == 1 && !Polymorph) ||
                                (change == 2 && u.ulycn == NON_PM))
                            change = 0;
                        if(Polymorph && !rn2(100))
                            change = 1;
                        else if (u.ulycn >= LOW_PM && !Upolyd &&
                                !rn2(80 - (20 * night())))
                            change = 2;
                        if (change && !Unchanging) {
                            if (multi >= 0) {
                                if (occupation)
                                    stop_occupation();
                                else
                                    nomul(0);
                                if (change == 1) polyself(false);
                                else you_were();
                                change = 0;
                            }
                        }
                    }

                    if(Searching && multi >= 0) (void) dosearch0(1);
                    dosounds();
                    do_storms();
                    gethungry();
                    age_spells();
                    exerchk();
                    invault();
                    if (u.uhave.amulet) amulet();
                    if (!rn2(40+(int)(ACURR(A_DEX)*3)))
                        u_wipe_engr(rnd(3));
                    if (u.uevent.udemigod && !u.uinvulnerable) {
                        if (u.udg_cnt) u.udg_cnt--;
                        if (!u.udg_cnt) {
                            intervene();
                            u.udg_cnt = rn1(200, 50);
                        }
                    }
                    restore_attrib();
                    /* underwater and waterlevel vision are done here */
                    if (Is_waterlevel(&u.uz))
                        movebubbles();
                    else if (Underwater)
                        under_water(0);
                    /* vision while buried done here */
                    else if (u.uburied) under_ground(0);

                    /* when immobile, count is in turns */
                    if(multi < 0) {
                        if (++multi == 0) {     /* finished yet? */
                            unmul((char *)0);
                            /* if unmul caused a level change, take it now */
                            if (u.utotype) deferred_goto();
                        }
                    }
                }
            } while (youmonst.movement<NORMAL_SPEED); /* hero can't move loop */

            /******************************************/
            /* once-per-hero-took-time things go here */
            /******************************************/


        } /* actual time passed */

        /****************************************/
        /* once-per-player-input things go here */
        /****************************************/

        find_ac();
        if(!flags.mv || Blind) {
            /* redo monsters if hallu or wearing a helm of telepathy */
            if (Hallucination) {        /* update screen randomly */
                see_monsters();
                see_objects();
                see_traps();
                if (u.uswallow) swallowed(0);
            } else if (Unblind_telepat) {
                see_monsters();
            } else if (Warning || Warn_of_mon)
                see_monsters();

            if (vision_full_recalc) vision_recalc(0);   /* vision! */
        }
        if(flags.botl || flags.botlx) bot();

        flags.move = 1;

        if(multi >= 0 && occupation) {
            if ((*occupation)() == 0)
                occupation = 0;
            if (monster_nearby()) {
                stop_occupation();
                reset_eat();
            }
            continue;
        }

        if ((u.uhave.amulet || Clairvoyant) &&
                !In_endgame(&u.uz) && !BClairvoyant &&
                !(moves % 15) && !rn2(2))
            do_vicinity_map();

        if(u.utrap && u.utraptype == TT_LAVA) {
            if(!is_lava(u.ux,u.uy))
                u.utrap = 0;
            else if (!u.uinvulnerable) {
                u.utrap -= 1<<8;
                if(u.utrap < 1<<8) {
                    killer_format = KILLED_BY;
                    killer = "molten lava";
                    You("sink below the surface and die.");
                    done(DISSOLVED);
                } else if(didmove && !u.umoved) {
                    Norep("You sink deeper into the lava.");
                    u.utrap += rnd(4);
                }
            }
        }

        if (iflags.sanity_check)
            sanity_check();


        u.umoved = false;

        if (multi > 0) {
            lookaround();
            if (!multi) {
                /* lookaround may clear multi */
                flags.move = 0;
                if (flags.time) flags.botl = 1;
                continue;
            }
            if (flags.mv) {
                if(multi < COLNO && !--multi)
                    flags.travel = iflags.travel1 = flags.mv = flags.run = 0;
                domove();
            } else {
                --multi;
                rhack(save_cm);
            }
        } else if (multi == 0) {
            ckmailstatus();
            rhack((char *)0);
        }
        if (u.utotype)          /* change dungeon level */
            deferred_goto();    /* after rhack() */
        /* !flags.move here: multiple movement command stopped */
        else if (flags.time && (!flags.move || !flags.mv))
            flags.botl = 1;

        if (vision_full_recalc) vision_recalc(0);       /* vision! */
        /* when running in non-tport mode, this gets done through domove() */
        if ((!flags.run || iflags.runmode == RUN_TPORT) &&
                (multi && (!flags.travel ? !(multi % 7) : !(moves % 7L)))) {
            if (flags.time && flags.run) flags.botl = 1;
            display_nhwindow(WIN_MAP, false);
        }
    }
}

void stop_occupation (void) {
    if(occupation) {
        if (!maybe_finished_meal(true))
            You("stop %s.", occtxt);
        occupation = 0;
        flags.botl = 1; /* in case u.uhs changed */
        /* fainting stops your occupation, there's no reason to sync.
           sync_hunger();
           */
        nomul(0);
        pushch(0);
    }
}

void display_gamewindows(void) {
    WIN_MESSAGE = create_nhwindow(NHW_MESSAGE);
    WIN_STATUS = create_nhwindow(NHW_STATUS);
    WIN_MAP = create_nhwindow(NHW_MAP);
    WIN_INVEN = create_nhwindow(NHW_MENU);

    /*
     * The mac port is not DEPENDENT on the order of these
     * displays, but it looks a lot better this way...
     */
    display_nhwindow(WIN_STATUS, false);
    display_nhwindow(WIN_MESSAGE, false);
    clear_glyph_buffer();
    display_nhwindow(WIN_MAP, false);
}

void newgame(void) {
    int i;

    flags.ident = 1;

    for (i = 0; i < NUMMONS; i++)
        mvitals[i].mvflags = mons[i].geno & G_NOCORPSE;

    init_objects();         /* must be before u_init() */

    flags.pantheon = -1;    /* role_init() will reset this */
    role_init();            /* must be before init_dungeons(), u_init(),
                             * and init_artifacts() */

    init_dungeons();        /* must be before u_init() to avoid rndmonst()
                             * creating odd monsters for any tins and eggs
                             * in hero's initial inventory */
    init_artifacts();       /* before u_init() in case $WIZKIT specifies
                             * any artifacts */
    u_init();

    (void) signal(SIGINT, done1);
    if(iflags.news) display_file(NEWS, false);
    load_qtlist();  /* load up the quest text info */
    /*      quest_init();*/ /* Now part of role_init() */

    mklev();
    u_on_upstairs();
    vision_reset();         /* set up internals for level (after mklev) */
    check_special_room(false);

    flags.botlx = 1;

    /* Move the monster from under you or else
     * makedog() will fail when it calls makemon().
     *                      - ucsfcgl!kneller
     */
    if(MON_AT(u.ux, u.uy)) mnexto(m_at(u.ux, u.uy));
    (void) makedog();
    docrt();

    if (flags.legacy) {
        flush_screen(1);
        com_pager(1);
    }

    save_currentstate();
    program_state.something_worth_saving++; /* useful data now exists */

    /* Success! */
    welcome(true);
    return;
}

/* show "welcome [back] to nethack" message at program startup */
/* false => restoring an old game */
void welcome(bool new_game) {
    char buf[BUFSZ];
    bool currentgend = Upolyd ? u.mfemale : flags.female;

    /*
     * The "welcome back" message always describes your innate form
     * even when polymorphed or wearing a helm of opposite alignment.
     * Alignment is shown unconditionally for new games; for restores
     * it's only shown if it has changed from its original value.
     * Sex is shown for new games except when it is redundant; for
     * restores it's only shown if different from its original value.
     */
    *buf = '\0';
    if (new_game || u.ualignbase[A_ORIGINAL] != u.ualignbase[A_CURRENT])
        sprintf(eos(buf), " %s", align_str(u.ualignbase[A_ORIGINAL]));
    if (!urole.name.f &&
            (new_game ? (urole.allow & ROLE_GENDMASK) == (ROLE_MALE|ROLE_FEMALE) :
             currentgend != flags.initgend))
        sprintf(eos(buf), " %s", genders[currentgend].adj);

    pline(new_game ? "%s %s, welcome to NetHack!  You are a%s %s %s."
            : "%s %s, the%s %s %s, welcome back to NetHack!",
            Hello((struct monst *) 0), plname, buf, urace.adj,
            (currentgend && urole.name.f) ? urole.name.f : urole.name.m);
}

int main (int argc, char *argv[]) {
    int fd;
    bool exact_username;

    hname = argv[0];
    hackpid = getpid();
    (void) umask(0777 & ~0660);

    choose_windows("tty");

    if(argc > 1) {
        /*
         * Now we know the directory containing 'record' and
         * may do a prscore().  Exclude `-style' - it's a Qt option.
         */
        if (!strncmp(argv[1], "-s", 2) && strncmp(argv[1], "-style", 6)) {
            prscore(argc, argv);
            exit(EXIT_SUCCESS);
        }
    }

    /*
     * Change directories before we initialize the window system so
     * we can find the tile file.
     */

    check_linux_console();
    initoptions();
    init_nhwindows(&argc,argv);
    exact_username = whoami();
    init_linux_cons();

    /*
     * It seems you really want to play.
     */
    u.uhp = 1;      /* prevent RIP on early quits */
    signal(SIGHUP, hangup);
    signal(SIGXCPU, hangup);

    process_options(argc, argv);    /* command line options */

    getmailstatus();
    if (wizard)
        strcpy(plname, "wizard");
    else
        if(!*plname || !strncmp(plname, "player", 4)
                || !strncmp(plname, "games", 4)) {
            askname();
        } else if (exact_username) {
            /* guard against user names with hyphens in them */
            int len = strlen(plname);
            /* append the current role, if any, so that last dash is ours */
            if (++len < sizeof plname)
                strncat(strcat(plname, "-"), pl_character, sizeof plname - len - 1);
        }
    plnamesuffix();         /* strip suffix from name; calls askname() */
    /* again if suffix was whole name */
    /* accepts any suffix */
    if(!wizard) {
        /*
         * check for multiple games under the same name
         * (if !locknum) or check max nr of players (otherwise)
         */
        (void) signal(SIGQUIT,SIG_IGN);
        (void) signal(SIGINT,SIG_IGN);
        if(!locknum)
            sprintf(lock, "%d%s", (int)getuid(), plname);
        getlock();
    } else {
        sprintf(lock, "%d%s", (int)getuid(), plname);
        getlock();
    }

    dlb_init();     /* must be before newgame() */

    /*
     * Initialization of the boundaries of the mazes
     * Both boundaries have to be even.
     */
    x_maze_max = COLNO-1;
    if (x_maze_max % 2)
        x_maze_max--;
    y_maze_max = ROWNO-1;
    if (y_maze_max % 2)
        y_maze_max--;

    /*
     *  Initialize the vision system.  This must be before mklev() on a
     *  new game or before a level restore on a saved game.
     */
    vision_init();

    display_gamewindows();

    if ((fd = restore_saved_game()) >= 0) {
        /* Since wizard is actually flags.debug, restoring might
         * overwrite it.
         */
        bool remember_wiz_mode = wizard;
        const char *fq_save = fqname(SAVEF, SAVEPREFIX, 1);

        (void) chmod(fq_save,0);        /* disallow parallel restores */
        (void) signal(SIGINT, done1);
        if(iflags.news) {
            display_file(NEWS, false);
            iflags.news = false; /* in case dorecover() fails */
        }
        pline("Restoring save file...");
        mark_synch();   /* flush output */
        if(!dorecover(fd))
            goto not_recovered;
        if(!wizard && remember_wiz_mode) wizard = true;
        check_special_room(false);
        wd_message();

        if (discover || wizard) {
            if(yn("Do you want to keep the save file?") == 'n')
                (void) delete_savefile();
            else {
                (void) chmod(fq_save,0660); /* back to readable */
            }
        }
        flags.move = 0;
    } else {
not_recovered:
        player_selection();
        newgame();
        wd_message();

        flags.move = 0;
        set_wear();
        (void) pickup(1);
    }

    moveloop();
    exit(EXIT_SUCCESS);
    /*NOTREACHED*/
    return(0);
}

static void process_options (int argc, char *argv[]) {
    int i;


    /*
     * Process options.
     */
    while(argc > 1 && argv[1][0] == '-'){
        argv++;
        argc--;
        switch(argv[0][1]){
            case 'D':
                wizard = true;
                break;
            case 'X':
                discover = true;
                break;
            case 'n':
                iflags.news = false;
                break;
            case 'u':
                if(argv[0][2])
                    (void) strncpy(plname, argv[0]+2, sizeof(plname)-1);
                else if(argc > 1) {
                    argc--;
                    argv++;
                    (void) strncpy(plname, argv[0], sizeof(plname)-1);
                } else
                    raw_print("Player name expected after -u");
                break;
            case 'I':
            case 'i':
                if (!strncmpi(argv[0]+1, "IBM", 3))
                    switch_graphics(IBM_GRAPHICS);
                break;
                /*  case 'D': */
            case 'd':
                if (!strncmpi(argv[0]+1, "DEC", 3))
                    switch_graphics(DEC_GRAPHICS);
                break;
            case 'p': /* profession (role) */
                if (argv[0][2]) {
                    if ((i = str2role(&argv[0][2])) >= 0)
                        flags.initrole = i;
                } else if (argc > 1) {
                    argc--;
                    argv++;
                    if ((i = str2role(argv[0])) >= 0)
                        flags.initrole = i;
                }
                break;
            case 'r': /* race */
                if (argv[0][2]) {
                    if ((i = str2race(&argv[0][2])) >= 0)
                        flags.initrace = i;
                } else if (argc > 1) {
                    argc--;
                    argv++;
                    if ((i = str2race(argv[0])) >= 0)
                        flags.initrace = i;
                }
                break;
            case '@':
                flags.randomall = 1;
                break;
            default:
                if ((i = str2role(&argv[0][1])) >= 0) {
                    flags.initrole = i;
                    break;
                }
                /* else raw_printf("Unknown option: %s", *argv); */
        }
    }

    if(argc > 1)
        locknum = atoi(argv[1]);
}

static bool whoami(void) {
    /*
     * Who am i? Algorithm: 1. Use name as specified in NETHACKOPTIONS
     *                      2. Use $USER or $LOGNAME        (if 1. fails)
     *                      3. Use getlogin()               (if 2. fails)
     * The resulting name is overridden by command line options.
     * If everything fails, or if the resulting name is some generic
     * account like "games", "play", "player", "hack" then eventually
     * we'll ask him.
     * Note that we trust the user here; it is possible to play under
     * somebody else's name.
     */
    char *s;

    if (*plname) return false;
    if(/* !*plname && */ (s = nh_getenv("USER")))
        (void) strncpy(plname, s, sizeof(plname)-1);
    if(!*plname && (s = nh_getenv("LOGNAME")))
        (void) strncpy(plname, s, sizeof(plname)-1);
    if(!*plname && (s = getlogin()))
        (void) strncpy(plname, s, sizeof(plname)-1);
    return true;
}

static void wd_message (void) {
    if (discover)
        You("are in non-scoring discovery mode.");
}
