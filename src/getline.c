/* See LICENSE in the root of this project for change info */
#include "hack.h"
#include "extern.h"
#include "winprocs.h"
#include "flag.h"


#include "wintty.h"
#include "func_tab.h"

char morc = 0;  /* tell the outside world what char you chose */
static bool ext_cmd_getlin_hook(char *);

typedef bool (*getlin_hook_proc)(char *);

static void hooked_tty_getlin(const char*,char*,getlin_hook_proc);
extern int extcmd_via_menu(void);       /* cmd.c */

extern char erase_char, kill_char;      /* from appropriate tty.c file */


/*
 * Read a line closed with '\n' into the array char bufp[BUFSZ].
 * (The '\n' is not stored. The string is closed with a '\0'.)
 * Reading can be interrupted by an escape ('\033') - now the
 * resulting string is "\033".
 */
void
tty_getlin (const char *query, char *bufp)
{
    hooked_tty_getlin(query, bufp, (getlin_hook_proc) 0);
}

static void 
hooked_tty_getlin (const char *query, char *bufp, getlin_hook_proc hook)
{
        char *obufp = bufp;
        int c;
        struct WinDesc *cw = wins[WIN_MESSAGE];
        bool doprev = 0;

        if(ttyDisplay->toplin == 1 && !(cw->flags & WIN_STOP)) more();
        cw->flags &= ~WIN_STOP;
        ttyDisplay->toplin = 3; /* special prompt state */
        ttyDisplay->inread++;
        pline("%s ", query);
        *obufp = 0;
        for(;;) {
                (void) fflush(stdout);
                sprintf(toplines, "%s ", query);
                strcat(toplines, obufp);
                if((c = pgetchar()) == EOF) {
                        break;
                }
                if(c == '\033') {
                        *obufp = c;
                        obufp[1] = 0;
                        break;
                }
                if (ttyDisplay->intr) {
                    ttyDisplay->intr--;
                    *bufp = 0;
                }
                if(c == '\020') { /* ctrl-P */
                    if (iflags.prevmsg_window != 's') {
                        int sav = ttyDisplay->inread;
                        ttyDisplay->inread = 0;
                        (void) tty_doprev_message();
                        ttyDisplay->inread = sav;
                        tty_clear_nhwindow(WIN_MESSAGE);
                        cw->maxcol = cw->maxrow;
                        addtopl(query);
                        addtopl(" ");
                        *bufp = 0;
                        addtopl(obufp);
                    } else {
                        if (!doprev)
                            (void) tty_doprev_message();/* need two initially */
                        (void) tty_doprev_message();
                        doprev = 1;
                        continue;
                    }
                } else if (doprev && iflags.prevmsg_window == 's') {
                    tty_clear_nhwindow(WIN_MESSAGE);
                    cw->maxcol = cw->maxrow;
                    doprev = 0;
                    addtopl(query);
                    addtopl(" ");
                    *bufp = 0;
                    addtopl(obufp);
                }
                if(c == erase_char || c == '\b') {
                        if(bufp != obufp) {
                                char *i;

                                bufp--;
                                putsyms("\b");
                                for (i = bufp; *i; ++i) putsyms(" ");
                                for (; i > bufp; --i) putsyms("\b");
                                *bufp = 0;
                        } else  tty_nhbell();
                } else if(c == '\n') {
                        break;
                } else if(' ' <= (unsigned char) c && c != '\177' &&
                            (bufp-obufp < BUFSZ-1 && bufp-obufp < COLNO)) {
                                /* avoid isprint() - some people don't have it
                                   ' ' is not always a printing char */
                        char *i = eos(bufp);

                        *bufp = c;
                        bufp[1] = 0;
                        putsyms(bufp);
                        bufp++;
                        if (hook && (*hook)(obufp)) {
                            putsyms(bufp);
                            /* pointer and cursor left where they were */
                            for (i = bufp; *i; ++i) putsyms("\b");
                        } else if (i > bufp) {
                            char *s = i;

                            /* erase rest of prior guess */
                            for (; i > bufp; --i) putsyms(" ");
                            for (; s > bufp; --s) putsyms("\b");
                        }
                } else if(c == kill_char || c == '\177') { /* Robert Viduya */
                                /* this test last - @ might be the kill_char */
                        for (; *bufp; ++bufp) putsyms(" ");
                        for (; bufp != obufp; --bufp) putsyms("\b \b");
                        *bufp = 0;
                } else
                        tty_nhbell();
        }
        ttyDisplay->toplin = 2;         /* nonempty, no --More-- required */
        ttyDisplay->inread--;
        clear_nhwindow(WIN_MESSAGE);    /* clean up after ourselves */
}

void
xwaitforspace (
    const char *s       /* chars allowed besides return */
)
{
    int c, x = ttyDisplay ? (int) ttyDisplay->dismiss_more : '\n';

    morc = 0;

    while((c = tty_nhgetch()) != '\n') {
        if(iflags.cbreak) {
            if ((s && index(s,c)) || c == x) {
                morc = (char) c;
                break;
            }
            tty_nhbell();
        }
    }

}


/*
 * Implement extended command completion by using this hook into
 * tty_getlin.  Check the characters already typed, if they uniquely
 * identify an extended command, expand the string to the whole
 * command.
 *
 * Return true if we've extended the string at base.  Otherwise return false.
 * Assumptions:
 *
 *      + we don't change the characters that are already in base
 *      + base has enough room to hold our string
 */
static bool 
ext_cmd_getlin_hook (char *base)
{
        int oindex, com_index;

        com_index = -1;
        for (oindex = 0; extcmdlist[oindex].ef_txt != (char *)0; oindex++) {
                if (!strncmpi(base, extcmdlist[oindex].ef_txt, strlen(base))) {
                        if (com_index == -1)    /* no matches yet */
                            com_index = oindex;
                        else                    /* more than 1 match */
                            return false;
                }
        }
        if (com_index >= 0) {
                strcpy(base, extcmdlist[com_index].ef_txt);
                return true;
        }

        return false;   /* didn't match anything */
}

/*
 * Read in an extended command, doing command line completion.  We
 * stop when we have found enough characters to make a unique command.
 */
int
tty_get_ext_cmd (void)
{
        int i;
        char buf[BUFSZ];

        if (iflags.extmenu) return extcmd_via_menu();
        /* maybe a runtime option? */
        /* hooked_tty_getlin("#", buf, flags.cmd_comp ? ext_cmd_getlin_hook : (getlin_hook_proc) 0); */
        hooked_tty_getlin("#", buf, in_doagain ? (getlin_hook_proc)0
                : ext_cmd_getlin_hook);
        (void) mungspaces(buf);
        if (buf[0] == 0 || buf[0] == '\033') return -1;

        for (i = 0; extcmdlist[i].ef_txt != (char *)0; i++)
                if (!strcmpi(buf, extcmdlist[i].ef_txt)) break;

        if (!in_doagain) {
            int j;
            for (j = 0; buf[j]; j++)
                savech(buf[j]);
            savech('\n');
        }

        if (extcmdlist[i].ef_txt == (char *)0) {
                pline("%s: unknown extended command.", buf);
                i = -1;
        }

        return i;
}


