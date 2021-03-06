/* See LICENSE in the root of this project for change info */
#ifndef WINTTY_H
#define WINTTY_H

#include "global.h"
#include "wintype.h"

/* menu structure */
typedef struct tty_mi {
    struct tty_mi *next;
    anything identifier;        /* user identifier */
    long count;                 /* user count */
    char *str;                  /* description string (including accelerator) */
    int attr;                   /* string attribute */
    bool selected;           /* true if selected by user */
    char selector;              /* keyboard accelerator */
    char gselector;             /* group accelerator */
} tty_menu_item;

/* descriptor for tty-based windows */
struct WinDesc {
    int flags;                  /* window flags */
    signed char type;                   /* type of window */
    bool active;             /* true if window is active */
    unsigned char offx, offy;           /* offset from topleft of display */
    short rows, cols;           /* dimensions */
    short curx, cury;           /* current cursor position */
    short maxrow, maxcol;       /* the maximum size used -- for MENU wins */
                                /* maxcol is also used by WIN_MESSAGE for */
                                /* tracking the ^P command */
    short *datlen;              /* allocation size for *data */
    char **data;                /* window data [row][column] */
    char *morestr;              /* string to display instead of default */
    tty_menu_item *mlist;       /* menu information (MENU) */
    tty_menu_item **plist;      /* menu page pointers (MENU) */
    short plist_size;           /* size of allocated plist (MENU) */
    short npages;               /* number of pages in menu (MENU) */
    short nitems;               /* total number of items (MENU) */
    short how;                  /* menu mode - pick 1 or N (MENU) */
    char menu_ch;               /* menu char (MENU) */
};

/* window flags */
#define WIN_CANCELLED 1
#define WIN_STOP 1              /* for NHW_MESSAGE; stops output */

/* descriptor for tty-based displays -- all the per-display data */
struct DisplayDesc {
    unsigned char rows, cols;           /* width and height of tty display */
    unsigned char curx, cury;           /* current cursor position on the screen */
    int color;                  /* current color */
    int attrs;                  /* attributes in effect */
    int toplin;                 /* flag for topl stuff */
    int rawprint;               /* number of raw_printed lines since synch */
    int inmore;                 /* non-zero if more() is active */
    int inread;                 /* non-zero if reading a character */
    int intr;                   /* non-zero if inread was interrupted */
    winid lastwin;              /* last window used for I/O */
    char dismiss_more;          /* extra character accepted at --More-- */
};

#define MAXWIN 20               /* maximum number of windows, cop-out */

/* tty dependent window types */
#define NHW_BASE    6

extern struct window_procs tty_procs;

/* port specific variable declarations */
extern winid BASE_WINDOW;

extern struct WinDesc *wins[MAXWIN];

extern struct DisplayDesc *ttyDisplay;  /* the tty display descriptor */

extern char morc;               /* last character typed to xwaitforspace */
extern char defmorestr[];       /* default --more-- prompt */

/* port specific external function references */

/* ### getline.c ### */
extern void xwaitforspace(const char *);

/* ### termcap.c, video.c ### */

extern void tty_startup(int*, int*);
extern void tty_shutdown(void);
extern void xputc(char);
extern void xputs(const char *);
extern void cl_end(void);
extern void clear_screen(void);
extern void home(void);
extern void standoutbeg(void);
extern void standoutend(void);
extern void backsp(void);
extern void graph_on(void);
extern void graph_off(void);
extern void cl_eos(void);

/*
 * termcap.c (or facsimiles in other ports) is the right place for doing
 * strange and arcane things such as outputting escape sequences to select
 * a color or whatever.  wintty.c should concern itself with WHERE to put
 * stuff in a window.
 */
extern void term_start_attr(int attr);
extern void term_end_attr(int attr);
extern void term_start_raw_bold(void);
extern void term_end_raw_bold(void);

extern void term_end_color(void);
extern void term_start_color(int color);
extern int has_color(int color);


/* ### topl.c ### */

extern void addtopl(const char *);
extern void more(void);
extern void update_topl(const char *);
extern void putsyms(const char*);

/* ### wintty.c ### */
extern void docorner(int, int);
extern void end_glyphout(void);
extern void g_putch(int);
extern void win_tty_init(void);

/* external declarations */
extern void tty_init_nhwindows(int *, char **);
extern void tty_player_selection(void);
extern void tty_askname(void);
extern void tty_get_nh_event(void) ;
extern void tty_exit_nhwindows(const char *);
extern void tty_suspend_nhwindows(const char *);
extern void tty_resume_nhwindows(void);
extern winid tty_create_nhwindow(int);
extern void tty_clear_nhwindow(winid);
extern void tty_display_nhwindow(winid, bool);
extern void tty_dismiss_nhwindow(winid);
extern void tty_destroy_nhwindow(winid);
extern void tty_curs(winid,int,int);
extern void tty_putstr(winid, int, const char *);
extern void tty_display_file(const char *, bool);
extern void tty_start_menu(winid);
extern void tty_add_menu(winid,int,const ANY_P *,
                        char,char,int,const char *, bool);
extern void tty_end_menu(winid, const char *);
extern int tty_select_menu(winid, int, MENU_ITEM_P **);
extern char tty_message_menu(char,int,const char *);
extern void tty_update_inventory(void);
extern void tty_mark_synch(void);
extern void tty_wait_synch(void);
extern void tty_print_glyph(winid,signed char,signed char,int);
extern void tty_raw_print(const char *);
extern void tty_raw_print_bold(const char *);
extern int tty_nhgetch(void);
extern int tty_nh_poskey(int *, int *, int *);
extern void tty_nhbell(void);
extern int tty_doprev_message(void);
extern char tty_yn_function(const char *, const char *, char);
extern void tty_getlin(const char *,char *);
extern int tty_get_ext_cmd(void);
extern void tty_number_pad(int);
extern void tty_delay_output(void);

/* other defs that really should go away (they're tty specific) */
extern void tty_start_screen(void);
extern void tty_end_screen(void);

extern void genl_outrip(winid,int);

#endif /* WINTTY_H */
