/* See LICENSE in the root of this project for change info */

#include "permonst.h"
#include "objclass.h"
#include "monsym.h"
#include "artilist.h"
#include "dungeon.h"
#include "obj.h"
#include "monst.h"
#include "you.h"
#include "flag.h"
#include "dlb.h"

/* version information */
#include "patchlevel.h"

#define rewind(fp) fseek((fp),0L,SEEK_SET)      /* guarantee a return value */


        /* names of files to be generated */
#define DATE_FILE       "date.h"
#define OPTIONS_FILE    "options"
#define ORACLE_FILE     "oracles"
#define DATA_FILE       "data"
#define RUMOR_FILE      "rumors"
#define DGN_I_FILE      "dungeon.def"
#define DGN_O_FILE      "dungeon.pdf"
#define MON_STR_C       "monstr.c"
#define QTXT_I_FILE     "quest.txt"
#define QTXT_O_FILE     "quest.dat"
/* locations for those files */
#define OUTPUT_FILE_PATH_TEMPLATE "../build/%s"
#define DATA_IN_TEMPLATE "../dat/%s"

static const char
    *Dont_Edit_Code =
        "/* This source file is generated by 'makedefs'.  Do not edit. */\n",
    *Dont_Edit_Data =
        "#\tThis data file is generated by 'makedefs'.  Do not edit. \n";

static struct version_info version;

/* definitions used for vision tables */
#define TEST_WIDTH  COLNO
#define TEST_HEIGHT ROWNO
#define BLOCK_WIDTH (TEST_WIDTH + 10)
#define BLOCK_HEIGHT TEST_HEIGHT        /* don't need extra spaces */
#define MAX_ROW (BLOCK_HEIGHT + TEST_HEIGHT)
#define MAX_COL (BLOCK_WIDTH + TEST_WIDTH)
/* Use this as an out-of-bound value in the close table.  */
#define CLOSE_OFF_TABLE_STRING "99"     /* for the close table */
#define FAR_OFF_TABLE_STRING "0xff"     /* for the far table */

#define sign(z) ((z) < 0 ? -1 : ((z) ? 1 : 0))
/*-end of vision defs-*/

static char     in_line[256], filename[60];

int main(int,char **);
void do_makedefs(char *);
void do_data(void);
void do_date(void);
void do_options(void);
void do_monstr(void);
void do_questtxt(void);
void do_rumors(void);
void do_oracles(void);

extern void monst_init(void);           /* monst.c */
extern void objects_init(void); /* objects.c */

static void make_version(void);
static char *version_string(char *);
static char *version_id_string(char *,const char *);
static char *xcrypt(const char *);
static int check_control(char *);
static bool d_filter(char *);
static bool h_filter(char *);
static bool ranged_attk(struct permonst*);
static int mstrength(struct permonst *);
static void build_savebones_compat_string(void);

static bool qt_comment(char *);
static bool qt_control(char *);
static int get_hdr(char *);
static bool new_id(char *);
static bool known_msg(int,int);
static void new_msg(char *,int,int);
static void do_qt_control(char *);
static void do_qt_text(char *);
static void adjust_qt_hdrs(void);
static void put_qt_hdrs(void);

static char *tmpdup(const char *);
static char *eos(char *);

/* input, output, tmp */
static FILE *ifp, *ofp, *tfp;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Bad arg count (%d).\n", argc - 1);
        fflush(stderr);
        return 1;
    }

    do_makedefs(&argv[1][1]);
    return 0;
}

void
do_makedefs (char *options)
{
        bool more_than_one;

        /* Note:  these initializers don't do anything except guarantee that
                we're linked properly.
        */
        monst_init();
        objects_init();

        /* construct the current version number */
        make_version();


        more_than_one = strlen(options) > 1;
        while (*options) {
            if (more_than_one)
                fprintf(stderr, "makedefs -%c\n", *options);

            switch (*options) {
                case 'd':
                case 'D':       do_data();
                                break;
                case 'm':
                case 'M':       do_monstr();
                                break;
                case 'v':
                case 'V':       do_date();
                                do_options();
                                break;
                case 'q':
                case 'Q':       do_questtxt();
                                break;
                case 'r':
                case 'R':       do_rumors();
                                break;
                case 'h':
                case 'H':       do_oracles();
                                break;
                default:        fprintf(stderr, "Unknown option '%c'.\n",
                                        *options);
                                (void) fflush(stderr);
                                exit(EXIT_FAILURE);

            }
            options++;
        }
        if (more_than_one) fprintf(stderr, "Completed.\n");     /* feedback */

}


/* trivial text encryption routine which can't be broken with `tr' */
/* duplicated in src/hacklib.c */
static char * xcrypt (const char *str) {
        static char buf[BUFSZ];
        const char *p;
        char *q;
        int bitmask;

        for (bitmask = 1, p = str, q = buf; *p; q++) {
                *q = *p++;
                if (*q & (32|64)) *q ^= bitmask;
                if ((bitmask <<= 1) >= 32) bitmask = 1;
        }
        *q = '\0';
        return buf;
}

void do_rumors (void) {
        char    infile[60];
        long    true_rumor_size;

        filename[0]='\0';
        sprintf(eos(filename), OUTPUT_FILE_PATH_TEMPLATE, RUMOR_FILE);
        if (!(ofp = fopen(filename, "w+"))) {
                perror(filename);
                exit(EXIT_FAILURE);
        }
        fprintf(ofp, "%s", Dont_Edit_Data);

        sprintf(infile, DATA_IN_TEMPLATE, RUMOR_FILE);
        strcat(infile, ".tru");
        if (!(ifp = fopen(infile, "r"))) {
                perror(infile);
                fclose(ofp);
                unlink(filename);       /* kill empty output file */
                exit(EXIT_FAILURE);
        }

        /* get size of true rumors file */
        (void) fseek(ifp, 0L, SEEK_END);
        true_rumor_size = ftell(ifp);
        fprintf(ofp,"%06lx\n", true_rumor_size);
        (void) fseek(ifp, 0L, SEEK_SET);

        /* copy true rumors */
        while (fgets(in_line, sizeof in_line, ifp) != 0)
                (void) fputs(xcrypt(in_line), ofp);

        fclose(ifp);

        sprintf(infile, DATA_IN_TEMPLATE, RUMOR_FILE);
        strcat(infile, ".fal");
        if (!(ifp = fopen(infile, "r"))) {
                perror(infile);
                fclose(ofp);
                unlink(filename);       /* kill incomplete output file */
                exit(EXIT_FAILURE);
        }

        /* copy false rumors */
        while (fgets(in_line, sizeof in_line, ifp) != 0)
                (void) fputs(xcrypt(in_line), ofp);

        fclose(ifp);
        fclose(ofp);
        return;
}

static void make_version(void) {
        int i;

        /*
         * integer version number
         */
        version.incarnation = ((unsigned long)VERSION_MAJOR << 24) |
                                ((unsigned long)VERSION_MINOR << 16) |
                                ((unsigned long)PATCHLEVEL << 8) |
                                ((unsigned long)EDITLEVEL);
        /*
         * encoded feature list
         * Note:  if any of these magic numbers are changed or reassigned,
         * EDITLEVEL in patchlevel.h should be incremented at the same time.
         * The actual values have no special meaning, and the category
         * groupings are just for convenience.
         */
        version.feature_set = (unsigned long)(0L
                /* levels and/or topology (0..4) */
                        | (1L <<  2)
                /* monsters (5..9) */
                        | (1L <<  6)
                        | (1L <<  7)
                /* objects (10..14) */
                        | (1L << 10)
                        | (1L << 11)
                /* flag bits and/or other global variables (15..26) */
                        | (1L << 17)
                        | (1L << 18)
                        | (1L << 19)
                        | (1L << 20)
                        );
        /*
         * Value used for object & monster sanity check.
         *    (NROFARTIFACTS<<24) | (NUM_OBJECTS<<12) | (NUMMONS<<0)
         */
        for (i = 1; artilist[i].otyp; i++) continue;
        version.entity_count = (unsigned long)(i - 1);
        for (i = 1; objects[i].oc_class != ILLOBJ_CLASS; i++) continue;
        version.entity_count = (version.entity_count << 12) | (unsigned long)i;
        for (i = 0; mons[i].mlet; i++) continue;
        version.entity_count = (version.entity_count << 12) | (unsigned long)i;
        /*
         * Value used for compiler (word size/field alignment/padding) check.
         */
        version.struct_sizes = (((unsigned long)sizeof (struct flag)  << 24) |
                                ((unsigned long)sizeof (struct obj)   << 17) |
                                ((unsigned long)sizeof (struct monst) << 10) |
                                ((unsigned long)sizeof (struct you)));
        return;
}

static char * version_string(char *outbuf) {
    sprintf(outbuf, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, PATCHLEVEL);
    return outbuf;
}

static char * version_id_string (char *outbuf, const char *build_date) {
    char versbuf[64];
    sprintf(outbuf, "NetHack Version %s - last build %s.", version_string(versbuf), build_date);
    return outbuf;
}

void do_date (void) {
        long clocktim = 0;
        char *c, cbuf[60], buf[BUFSZ];
        const char *ul_sfx;

        filename[0]='\0';
        sprintf(eos(filename), OUTPUT_FILE_PATH_TEMPLATE, DATE_FILE);
        if (!(ofp = fopen(filename, "w+"))) {
                perror(filename);
                exit(EXIT_FAILURE);
        }
        fprintf(ofp,"/*\tSCCS Id: @(#)date.h\t3.4\t2002/02/03 */\n\n");
        fprintf(ofp, "%s", Dont_Edit_Code);

        (void) time((time_t *)&clocktim);
        strcpy(cbuf, ctime((time_t *)&clocktim));
        for (c = cbuf; *c; c++) if (*c == '\n') break;
        *c = '\0';      /* strip off the '\n' */
        fprintf(ofp,"#define BUILD_DATE \"%s\"\n", cbuf);
        fprintf(ofp,"#define BUILD_TIME (%ldL)\n", clocktim);
        fprintf(ofp,"\n");
        ul_sfx = "UL";
        fprintf(ofp,"#define VERSION_NUMBER 0x%08lx%s\n",
                version.incarnation, ul_sfx);
        fprintf(ofp,"#define VERSION_FEATURES 0x%08lx%s\n",
                version.feature_set, ul_sfx);
        fprintf(ofp,"#define VERSION_SANITY1 0x%08lx%s\n",
                version.entity_count, ul_sfx);
        fprintf(ofp,"#define VERSION_SANITY2 0x%08lx%s\n",
                version.struct_sizes, ul_sfx);
        fprintf(ofp,"\n");
        fprintf(ofp,"#define VERSION_STRING \"%s\"\n", version_string(buf));
        fprintf(ofp,"#define VERSION_ID \\\n \"%s\"\n",
                version_id_string(buf, cbuf));
        fprintf(ofp,"\n");
        fclose(ofp);
        return;
}

static char save_bones_compat_buf[BUFSZ];

static void build_savebones_compat_string (void) {
        strcpy(save_bones_compat_buf,
                "save and bones files accepted from version");
        sprintf(eos(save_bones_compat_buf), " %d.%d.%d only",
                VERSION_MAJOR, VERSION_MINOR, PATCHLEVEL);
}

static const char *build_opts[] = {
                "autopickup_exceptions",
                "color",
                "data librarian",
                "debug mode",
                "Elbereth",
                "experience points on status line",
                "insurance files for recovering from crashes",
                "Keystone Kops",
                "log file",
                "mail daemon",
                "news file",
                "redo command",
                "saddles and riding",
                "seduction",
                "sinks",
                "terminal info library",
                "tourists",
                "variable playground",
                "walled mazes",
                save_bones_compat_buf,
                "basic NetHack features"
        };

static const char *window_opts[] = {
                "traditional tty-based graphics",
                0
        };

void do_options(void) {
        int i, length;
        const char *str, *indent = "    ";

        filename[0]='\0';
        sprintf(eos(filename), OUTPUT_FILE_PATH_TEMPLATE, OPTIONS_FILE);
        if (!(ofp = fopen(filename, "w+"))) {
                perror(filename);
                exit(EXIT_FAILURE);
        }

        build_savebones_compat_string();
        fprintf(ofp,
                "\n    NetHack version %d.%d.%d\n",
                VERSION_MAJOR, VERSION_MINOR, PATCHLEVEL);

        fprintf(ofp,"\nOptions compiled into this edition:\n");

        length = COLNO + 1;     /* force 1st item onto new line */
        for (i = 0; i < SIZE(build_opts); i++) {
            str = build_opts[i];
            if (length + strlen(str) > COLNO - 5)
                fprintf(ofp,"\n%s", indent),  length = strlen(indent);
            else
                fprintf(ofp," "),  length++;
            fprintf(ofp,"%s", str),  length += strlen(str);
            fprintf(ofp,(i < SIZE(build_opts) - 1) ? "," : "."),  length++;
        }

        fprintf(ofp,"\n\nSupported windowing systems:\n");

        length = COLNO + 1;     /* force 1st item onto new line */
        for (i = 0; i < SIZE(window_opts) - 1; i++) {
            str = window_opts[i];
            if (length + strlen(str) > COLNO - 5)
                fprintf(ofp,"\n%s", indent),  length = strlen(indent);
            else
                fprintf(ofp," "),  length++;
            fprintf(ofp,"%s", str),  length += strlen(str);
            fprintf(ofp, ","),  length++;
        }
        fprintf(ofp, "\n%swith a default of %s.", indent, "tty");
        fprintf(ofp,"\n\n");

        fclose(ofp);
        return;
}

/* routine to decide whether to discard something from data.base */
static bool d_filter(char *line) {
    if (*line == '#') return true;      /* ignore comment lines */
    return false;
}

   /*
    *
        New format (v3.1) of 'data' file which allows much faster lookups [pr]
"do not edit"           first record is a comment line
01234567                hexadecimal formatted offset to text area
name-a                  first name of interest
123,4                   offset to name's text, and number of lines for it
name-b                  next name of interest
name-c                  multiple names which share same description also
456,7                   share a single offset,count line
.                       sentinel to mark end of names
789,0                   dummy record containing offset, count of EOF
text-a                  4 lines of descriptive text for name-a
text-a                  at file position 0x01234567L + 123L
text-a
text-a
text-b/text-c           7 lines of text for names-b and -c
text-b/text-c           at fseek(0x01234567L + 456L)
...
    *
    */

void do_data (void) {
        char    infile[60], tempfile[60];
        bool ok;
        long    txt_offset;
        int     entry_cnt, line_cnt;

        sprintf(tempfile, OUTPUT_FILE_PATH_TEMPLATE, "database.tmp");
        filename[0]='\0';
        sprintf(eos(filename), OUTPUT_FILE_PATH_TEMPLATE, DATA_FILE);
        sprintf(infile, DATA_IN_TEMPLATE, DATA_FILE);
        strcat(infile, ".base");
        if (!(ifp = fopen(infile, "r"))) {          /* data.base */
                perror(infile);
                exit(EXIT_FAILURE);
        }
        if (!(ofp = fopen(filename, "w+"))) {        /* data */
                perror(filename);
                fclose(ifp);
                exit(EXIT_FAILURE);
        }
        if (!(tfp = fopen(tempfile, "w+"))) {        /* database.tmp */
                perror(tempfile);
                fclose(ifp);
                fclose(ofp);
                unlink(filename);
                exit(EXIT_FAILURE);
        }

        /* output a dummy header record; we'll rewind and overwrite it later */
        fprintf(ofp, "%s%08lx\n", Dont_Edit_Data, 0L);

        entry_cnt = line_cnt = 0;
        /* read through the input file and split it into two sections */
        while (fgets(in_line, sizeof in_line, ifp)) {
            if (d_filter(in_line)) continue;
            if (*in_line > ' ') {       /* got an entry name */
                /* first finish previous entry */
                if (line_cnt)  fprintf(ofp, "%d\n", line_cnt),  line_cnt = 0;
                /* output the entry name */
                (void) fputs(in_line, ofp);
                entry_cnt++;            /* update number of entries */
            } else if (entry_cnt) {     /* got some descriptive text */
                /* update previous entry with current text offset */
                if (!line_cnt)  fprintf(ofp, "%ld,", ftell(tfp));
                /* save the text line in the scratch file */
                (void) fputs(in_line, tfp);
                line_cnt++;             /* update line counter */
            }
        }
        /* output an end marker and then record the current position */
        if (line_cnt)  fprintf(ofp, "%d\n", line_cnt);
        fprintf(ofp, ".\n%ld,%d\n", ftell(tfp), 0);
        txt_offset = ftell(ofp);
        fclose(ifp);            /* all done with original input file */

        /* reprocess the scratch file; 1st format an error msg, just in case */
        sprintf(in_line, "rewind of \"%s\"", tempfile);
        if (rewind(tfp) != 0)  goto dead_data;
        /* copy all lines of text from the scratch file into the output file */
        while (fgets(in_line, sizeof in_line, tfp))
            (void) fputs(in_line, ofp);

        /* finished with scratch file */
        fclose(tfp);
        unlink(tempfile);       /* remove it */

        /* update the first record of the output file; prepare error msg 1st */
        sprintf(in_line, "rewind of \"%s\"", filename);
        ok = (rewind(ofp) == 0);
        if (ok) {
           sprintf(in_line, "header rewrite of \"%s\"", filename);
           ok = (fprintf(ofp, "%s%08lx\n", Dont_Edit_Data, txt_offset) >= 0);
        }
        if (!ok) {
dead_data:  perror(in_line);    /* report the problem */
            /* close and kill the aborted output file, then give up */
            fclose(ofp);
            unlink(filename);
            exit(EXIT_FAILURE);
        }

        /* all done */
        fclose(ofp);

        return;
}

/* routine to decide whether to discard something from oracles.txt */
static bool h_filter(char *line) {
    if (*line == '#') return true;      /* ignore comment lines */
    return false;
}

static const char *special_oracle[] = {
        "\"...it is rather disconcerting to be confronted with the",
        "following theorem from [Baker, Gill, and Solovay, 1975].",
        "",
        "Theorem 7.18  There exist recursive languages A and B such that",
        "  (1)  P(A) == NP(A), and",
        "  (2)  P(B) != NP(B)",
        "",
        "This provides impressive evidence that the techniques that are",
        "currently available will not suffice for proving that P != NP or          ",
        "that P == NP.\"  [Garey and Johnson, p. 185.]"
};

/*
   The oracle file consists of a "do not edit" comment, a decimal count N
   and set of N+1 hexadecimal fseek offsets, followed by N multiple-line
   records, separated by "---" lines.  The first oracle is a special case.
   The input data contains just those multi-line records, separated by
   "-----" lines.
 */

void do_oracles (void) {
        char    infile[60], tempfile[60];
        bool in_oracle, ok;
        long    txt_offset, offset, fpos;
        int     oracle_cnt;
        int i;

        sprintf(tempfile, OUTPUT_FILE_PATH_TEMPLATE, "oracles.tmp");
        filename[0]='\0';
        sprintf(eos(filename), OUTPUT_FILE_PATH_TEMPLATE, ORACLE_FILE);
        sprintf(infile, DATA_IN_TEMPLATE, ORACLE_FILE);
        strcat(infile, ".txt");
        if (!(ifp = fopen(infile, "r"))) {
                perror(infile);
                exit(EXIT_FAILURE);
        }
        if (!(ofp = fopen(filename, "w+"))) {
                perror(filename);
                fclose(ifp);
                exit(EXIT_FAILURE);
        }
        if (!(tfp = fopen(tempfile, "w+"))) {        /* oracles.tmp */
                perror(tempfile);
                fclose(ifp);
                fclose(ofp);
                unlink(filename);
                exit(EXIT_FAILURE);
        }

        /* output a dummy header record; we'll rewind and overwrite it later */
        fprintf(ofp, "%s%5d\n", Dont_Edit_Data, 0);

        /* handle special oracle; it must come first */
        (void) fputs("---\n", tfp);
        fprintf(ofp, "%05lx\n", ftell(tfp));  /* start pos of special oracle */
        for (i = 0; i < SIZE(special_oracle); i++) {
            (void) fputs(xcrypt(special_oracle[i]), tfp);
            (void) fputc('\n', tfp);
        }

        oracle_cnt = 1;
        (void) fputs("---\n", tfp);
        fprintf(ofp, "%05lx\n", ftell(tfp));    /* start pos of first oracle */
        in_oracle = false;

        while (fgets(in_line, sizeof in_line, ifp)) {
            if (h_filter(in_line)) continue;
            if (!strncmp(in_line, "-----", 5)) {
                if (!in_oracle) continue;
                in_oracle = false;
                oracle_cnt++;
                (void) fputs("---\n", tfp);
                fprintf(ofp, "%05lx\n", ftell(tfp));
                /* start pos of this oracle */
            } else {
                in_oracle = true;
                (void) fputs(xcrypt(in_line), tfp);
            }
        }

        if (in_oracle) {        /* need to terminate last oracle */
            oracle_cnt++;
            (void) fputs("---\n", tfp);
            fprintf(ofp, "%05lx\n", ftell(tfp));        /* eof position */
        }

        /* record the current position */
        txt_offset = ftell(ofp);
        fclose(ifp);            /* all done with original input file */

        /* reprocess the scratch file; 1st format an error msg, just in case */
        sprintf(in_line, "rewind of \"%s\"", tempfile);
        if (rewind(tfp) != 0)  goto dead_data;
        /* copy all lines of text from the scratch file into the output file */
        while (fgets(in_line, sizeof in_line, tfp))
            (void) fputs(in_line, ofp);

        /* finished with scratch file */
        fclose(tfp);
        unlink(tempfile);       /* remove it */

        /* update the first record of the output file; prepare error msg 1st */
        sprintf(in_line, "rewind of \"%s\"", filename);
        ok = (rewind(ofp) == 0);
        if (ok) {
            sprintf(in_line, "header rewrite of \"%s\"", filename);
            ok = (fprintf(ofp, "%s%5d\n", Dont_Edit_Data, oracle_cnt) >=0);
        }
        if (ok) {
            sprintf(in_line, "data rewrite of \"%s\"", filename);
            for (i = 0; i <= oracle_cnt; i++) {
                if (!(ok = (fflush(ofp) == 0))) break;
                if (!(ok = (fpos = ftell(ofp)) >= 0)) break;
                if (!(ok = (fseek(ofp, fpos, SEEK_SET) >= 0))) break;
                if (!(ok = (fscanf(ofp, "%5lx", &offset) == 1))) break;
                if (!(ok = (fseek(ofp, fpos, SEEK_SET) >= 0))) break;
                if (!(ok = (fprintf(ofp, "%05lx\n", offset + txt_offset) >= 0)))
                    break;
            }
        }
        if (!ok) {
dead_data:  perror(in_line);    /* report the problem */
            /* close and kill the aborted output file, then give up */
            fclose(ofp);
            unlink(filename);
            exit(EXIT_FAILURE);
        }

        /* all done */
        fclose(ofp);

        return;
}


static  struct deflist {

        const char      *defname;
        bool true_or_false;
} deflist[] = {
              { 0, 0 } };

static int check_control (char *s) {
        int     i;

        if(s[0] != '%') return(-1);

        for(i = 0; deflist[i].defname; i++)
            if(!strncmp(deflist[i].defname, s+1, strlen(deflist[i].defname)))
                return(i);

        return(-1);
}

/* returns true if monster can attack at range */
static bool ranged_attk(struct permonst *ptr) {
        int     i, j;
        int atk_mask = (1<<AT_BREA) | (1<<AT_SPIT) | (1<<AT_GAZE);

        for(i = 0; i < NATTK; i++) {
            if((j=ptr->mattk[i].aatyp) >= AT_WEAP || (atk_mask & (1<<j)))
                return true;
        }

        return(false);
}

/* This routine is designed to return an integer value which represents
 * an approximation of monster strength.  It uses a similar method of
 * determination as "experience()" to arrive at the strength.
 */
static int mstrength (struct permonst *ptr) {
        int     i, tmp2, n, tmp = ptr->mlevel;

        if(tmp > 49)            /* special fixed hp monster */
            tmp = 2*(tmp - 6) / 4;

/*      For creation in groups */
        n = (!!(ptr->geno & G_SGROUP));
        n += (!!(ptr->geno & G_LGROUP)) << 1;

/*      For ranged attacks */
        if (ranged_attk(ptr)) n++;

/*      For higher ac values */
        n += (ptr->ac < 4);
        n += (ptr->ac < 0);

/*      For very fast monsters */
        n += (ptr->mmove >= 18);

/*      For each attack and "special" attack */
        for(i = 0; i < NATTK; i++) {

            tmp2 = ptr->mattk[i].aatyp;
            n += (tmp2 > 0);
            n += (tmp2 == AT_MAGC);
            n += (tmp2 == AT_WEAP && (ptr->mflags2 & M2_STRONG));
        }

/*      For each "special" damage type */
        for(i = 0; i < NATTK; i++) {

            tmp2 = ptr->mattk[i].adtyp;
            if ((tmp2 == AD_DRLI) || (tmp2 == AD_STON) || (tmp2 == AD_DRST)
                || (tmp2 == AD_DRDX) || (tmp2 == AD_DRCO) || (tmp2 == AD_WERE))
                        n += 2;
            else if (strcmp(ptr->mname, "grid bug")) n += (tmp2 != AD_PHYS);
            n += ((int) (ptr->mattk[i].damd * ptr->mattk[i].damn) > 23);
        }

/*      Leprechauns are special cases.  They have many hit dice so they
        can hit and are hard to kill, but they don't really do much damage. */
        if (!strcmp(ptr->mname, "leprechaun")) n -= 2;

/*      Finally, adjust the monster level  0 <= n <= 24 (approx.) */
        if(n == 0) tmp--;
        else if(n >= 6) tmp += ( n / 2 );
        else tmp += ( n / 3 + 1);

        return((tmp >= 0) ? tmp : 0);
}

void do_monstr (void) {
    struct permonst *ptr;
    int i, j;

    /*
     * create the source file, "monstr.c"
     */
    filename[0]='\0';
    sprintf(eos(filename), OUTPUT_FILE_PATH_TEMPLATE, MON_STR_C);
    if (!(ofp = fopen(filename, "w+"))) {
        perror(filename);
        exit(EXIT_FAILURE);
    }
    fprintf(ofp,"%s", Dont_Edit_Code);
    fprintf(ofp,"\nconst int monstr[] = {\n");
    for (ptr = &mons[0], j = 0; ptr->mlet; ptr++) {

        i = mstrength(ptr);
        fprintf(ofp,"%2d,%c", i, (++j & 15) ? ' ' : '\n');
    }
    /* might want to insert a final 0 entry here instead of just newline */
    fprintf(ofp,"%s};\n", (j & 15) ? "\n" : "");

    fprintf(ofp,"\nvoid monstr_init(void);\n");
    fprintf(ofp,"\nvoid\n");
    fprintf(ofp,"monstr_init()\n");
    fprintf(ofp,"{\n");
    fprintf(ofp,"    return;\n");
    fprintf(ofp,"}\n");
    fprintf(ofp,"\n/*monstr.c*/\n");

    fclose(ofp);
    return;
}

/*      Start of Quest text file processing. */
#include "qtmsg.h"

#define N_MSG   100             /* arbitrary */

struct msghdr {
        int     n_msg;
        struct  qtmsg   qt_msg[N_MSG];
};

struct  qthdr {
        int     n_hdr;
        char    id[N_HDR][LEN_HDR];
        long    offset[N_HDR];
};

/* Error message macros */
#define CREC_IN_MSG     "Control record encountered during message - line %d\n"
#define DUP_MSG         "Duplicate message number at line %d\n"
#define END_NOT_IN_MSG  "End record encountered before message - line %d\n"
#define TEXT_NOT_IN_MSG "Text encountered outside message - line %d\n"
#define UNREC_CREC      "Unrecognized Control record at line %d\n"
#define OUT_OF_HEADERS  "Too many message types (line %d)\nAdjust N_HDR in qtext.h and recompile.\n"
#define OUT_OF_MESSAGES "Too many messages in class (line %d)\nAdjust N_MSG in qtext.h and recompile.\n"


static struct qthdr     qt_hdr;
static struct msghdr    msg_hdr[N_HDR];
static struct qtmsg     *curr_msg;

static int      qt_line;

static bool  in_msg;
#define NO_MSG  1       /* strlen of a null line returned by fgets() */

static bool qt_comment(char *s) {
    if(s[0] == '#') return(true);
    return((bool)(!in_msg  && strlen(s) == NO_MSG));
}

static bool qt_control(char *s) {
    return((bool)(s[0] == '%' && (s[1] == 'C' || s[1] == 'E')));
}

static int get_hdr (char *code) {
        int     i;

        for(i = 0; i < qt_hdr.n_hdr; i++)
            if(!strncmp(code, qt_hdr.id[i], LEN_HDR)) return (++i);

        return(0);
}

static bool new_id (char *code) {
    if(qt_hdr.n_hdr >= N_HDR) {
        fprintf(stderr, OUT_OF_HEADERS, qt_line);
        return(false);
    }

    strncpy(&qt_hdr.id[qt_hdr.n_hdr][0], code, LEN_HDR);
    msg_hdr[qt_hdr.n_hdr].n_msg = 0;
    qt_hdr.offset[qt_hdr.n_hdr++] = 0L;
    return(true);
}

static bool known_msg(int num, int id) {
    int i;

    for(i = 0; i < msg_hdr[num].n_msg; i++)
        if(msg_hdr[num].qt_msg[i].msgnum == id) return(true);

    return(false);
}


static void new_msg (char *s, int num, int id) {
    struct  qtmsg   *qt_msg;

    if(msg_hdr[num].n_msg >= N_MSG) {
        fprintf(stderr, OUT_OF_MESSAGES, qt_line);
    } else {
        qt_msg = &(msg_hdr[num].qt_msg[msg_hdr[num].n_msg++]);
        qt_msg->msgnum = id;
        qt_msg->delivery = s[2];
        qt_msg->offset = qt_msg->size = 0L;

        curr_msg = qt_msg;
    }
}

static void do_qt_control (char *s) {
    char code[BUFSZ];
    int num, id = 0;

    switch(s[1]) {

        case 'C':   if(in_msg) {
                        fprintf(stderr, CREC_IN_MSG, qt_line);
                        break;
                    } else {
                        in_msg = true;
                        if (sscanf(&s[4], "%s %5d", code, &id) != 2) {
                            fprintf(stderr, UNREC_CREC, qt_line);
                            break;
                        }
                        num = get_hdr(code);
                        if (!num && !new_id(code))
                            break;
                        num = get_hdr(code)-1;
                        if(known_msg(num, id))
                            fprintf(stderr, DUP_MSG, qt_line);
                        else new_msg(s, num, id);
                    }
                    break;

        case 'E':   if(!in_msg) {
                        fprintf(stderr, END_NOT_IN_MSG, qt_line);
                        break;
                    } else in_msg = false;
                    break;

        default:    fprintf(stderr, UNREC_CREC, qt_line);
                    break;
    }
}

static void do_qt_text (char *s) {
        if (!in_msg) {
            fprintf(stderr, TEXT_NOT_IN_MSG, qt_line);
        }
        curr_msg->size += strlen(s);
        return;
}

static void adjust_qt_hdrs (void) {
        int     i, j;
        long count = 0L, hdr_offset = sizeof(int) +
                        (sizeof(char)*LEN_HDR + sizeof(long)) * qt_hdr.n_hdr;

        for(i = 0; i < qt_hdr.n_hdr; i++) {
            qt_hdr.offset[i] = hdr_offset;
            hdr_offset += sizeof(int) + sizeof(struct qtmsg) * msg_hdr[i].n_msg;
        }

        for(i = 0; i < qt_hdr.n_hdr; i++)
            for(j = 0; j < msg_hdr[i].n_msg; j++) {

                msg_hdr[i].qt_msg[j].offset = hdr_offset + count;
                count += msg_hdr[i].qt_msg[j].size;
            }
        return;
}

static void put_qt_hdrs (void) {
        int     i;

        /*
         *      The main header record.
         */
        (void) fwrite((void *)&(qt_hdr.n_hdr), sizeof(int), 1, ofp);
        (void) fwrite((void *)&(qt_hdr.id[0][0]), sizeof(char)*LEN_HDR,
                                                        qt_hdr.n_hdr, ofp);
        (void) fwrite((void *)&(qt_hdr.offset[0]), sizeof(long),
                                                        qt_hdr.n_hdr, ofp);

        /*
         *      The individual class headers.
         */
        for(i = 0; i < qt_hdr.n_hdr; i++) {

            (void) fwrite((void *)&(msg_hdr[i].n_msg), sizeof(int),
                                                        1, ofp);
            (void) fwrite((void *)&(msg_hdr[i].qt_msg[0]),
                            sizeof(struct qtmsg), msg_hdr[i].n_msg, ofp);
        }
}

void do_questtxt (void) {
        sprintf(filename, DATA_IN_TEMPLATE, QTXT_I_FILE);
        if(!(ifp = fopen(filename, "r"))) {
                perror(filename);
                exit(EXIT_FAILURE);
        }

        filename[0]='\0';
        sprintf(eos(filename), OUTPUT_FILE_PATH_TEMPLATE, QTXT_O_FILE);
        if(!(ofp = fopen(filename, "w+"))) {
                perror(filename);
                fclose(ifp);
                exit(EXIT_FAILURE);
        }

        qt_hdr.n_hdr = 0;
        qt_line = 0;
        in_msg = false;

        while (fgets(in_line, 80, ifp) != 0) {
            qt_line++;
            if(qt_control(in_line)) do_qt_control(in_line);
            else if(qt_comment(in_line)) continue;
            else                    do_qt_text(in_line);
        }

        (void) rewind(ifp);
        in_msg = false;
        adjust_qt_hdrs();
        put_qt_hdrs();
        while (fgets(in_line, 80, ifp) != 0) {

                if(qt_control(in_line)) {
                    in_msg = (in_line[1] == 'C');
                    continue;
                } else if(qt_comment(in_line)) continue;
                (void) fputs(xcrypt(in_line), ofp);
        }
        fclose(ifp);
        fclose(ofp);
        return;
}

static char * tmpdup (const char *str) {
        static char buf[128];

        if (!str) return (char *)0;
        (void)strncpy(buf, str, 127);
        return buf;
}

static char * eos (char *str) {
    while (*str) str++;
    return str;
}
