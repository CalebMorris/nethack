/* See LICENSE in the root of this project for change info */
/* not named termcap.h because it may conflict with a system header */

#ifndef TCAP_H
#define TCAP_H

#include "global.h"
#include "color.h"

extern struct tc_gbl_data {
    char *tc_AS, *tc_AE;        /* graphics start and end (tty font swapping) */
    int   tc_LI,  tc_CO;        /* lines and columns */
} tc_gbl_data;
#define AS tc_gbl_data.tc_AS
#define AE tc_gbl_data.tc_AE
#define LI tc_gbl_data.tc_LI
#define CO tc_gbl_data.tc_CO

extern struct tc_lcl_data {   /* defined and set up in termcap.c */
    char *tc_CM, *tc_ND, *tc_CD;
    char *tc_HI, *tc_HE, *tc_US, *tc_UE;
    bool tc_ul_hack;
} tc_lcl_data;
/* some curses.h declare CM etc. */
#define nh_CM tc_lcl_data.tc_CM
#define nh_ND tc_lcl_data.tc_ND
#define nh_CD tc_lcl_data.tc_CD
#define nh_HI tc_lcl_data.tc_HI
#define nh_HE tc_lcl_data.tc_HE
#define nh_US tc_lcl_data.tc_US
#define nh_UE tc_lcl_data.tc_UE
#define ul_hack tc_lcl_data.tc_ul_hack

extern short ospeed;            /* set up in termcap.c */

extern char *hilites[CLR_MAX];

#endif /* TCAP_H */
