/* See LICENSE in the root of this project for change info */
#ifndef EDOG_H
#define EDOG_H

#include "coord.h"

/*      various types of food, the lower, the better liked.     */

#define DOGFOOD 0
#define CADAVER 1
#define ACCFOOD 2
#define MANFOOD 3
#define APPORT  4
#define POISON  5
#define UNDEF   6
#define TABU    7

struct edog {
        long droptime;                  /* moment dog dropped object */
        unsigned dropdist;              /* dist of drpped obj from @ */
        int apport;                     /* amount of training */
        long whistletime;               /* last time he whistled */
        long hungrytime;                /* will get hungry at this time */
        coord ogoal;                    /* previous goal location */
        int abuse;                      /* track abuses to this pet */
        int revivals;                   /* count pet deaths */
        int mhpmax_penalty;             /* while starving, points reduced */
        unsigned killed_by_u: 1;       /* you attempted to kill him */
};
#define EDOG(mon)       ((struct edog *)&(mon)->mextra[0])

#endif /* EDOG_H */
