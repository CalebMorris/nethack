/* See LICENSE in the root of this project for change info */
#ifndef ENGRAVE_H
#define ENGRAVE_H

struct engr {
        struct engr *nxt_engr;
        char *engr_txt;
        signed char engr_x, engr_y;
        unsigned engr_lth;      /* for save & restore; not length of text */
        long engr_time;         /* moment engraving was (will be) finished */
        signed char engr_type;
#define DUST       1
#define ENGRAVE    2
#define BURN       3
#define MARK       4
#define ENGR_BLOOD 5
#define HEADSTONE  6
#define N_ENGRAVE  6
};

#define newengr(lth) (struct engr *)malloc((unsigned)(lth) + sizeof(struct engr))
#define dealloc_engr(engr) free((void *) (engr))

#endif /* ENGRAVE_H */
