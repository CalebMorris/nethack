/* See LICENSE in the root of this project for change info */
#ifndef COLOR_H
#define COLOR_H

/*
 * The color scheme used is tailored for an IBM PC.  It consists of the
 * standard 8 colors, folowed by their bright counterparts.  There are
 * exceptions, these are listed below.  Bright black doesn't mean very
 * much, so it is used as the "default" foreground color of the screen.
 */
enum {
    CLR_BLACK,
    CLR_RED,
    CLR_GREEN,
    CLR_BROWN, /* on IBM, low-intensity yellow is brown */
    CLR_BLUE,
    CLR_MAGENTA,
    CLR_CYAN,
    CLR_GRAY, /* low-intensity white */
    NO_COLOR,
    CLR_ORANGE,
    CLR_BRIGHT_GREEN,
    CLR_YELLOW,
    CLR_BRIGHT_BLUE,
    CLR_BRIGHT_MAGENTA,
    CLR_BRIGHT_CYAN,
    CLR_WHITE,
    CLR_MAX,
};

/* The "half-way" point for tty based color systems.  This is used in */
/* the tty color setup code.  (IMHO, it should be removed - dean).    */
enum {
    BRIGHT = 8,
};

/* these can be configured */
enum {
  HI_OBJ        = CLR_MAGENTA,
  HI_METAL      = CLR_CYAN,
  HI_COPPER     = CLR_YELLOW,
  HI_SILVER     = CLR_GRAY,
  HI_GOLD       = CLR_YELLOW,
  HI_LEATHER    = CLR_BROWN,
  HI_CLOTH      = CLR_BROWN,
  HI_ORGANIC    = CLR_BROWN,
  HI_WOOD       = CLR_BROWN,
  HI_PAPER      = CLR_WHITE,
  HI_GLASS      = CLR_BRIGHT_CYAN,
  HI_MINERAL    = CLR_GRAY,
  DRAGON_SILVER = CLR_BRIGHT_CYAN,
  HI_ZAP        = CLR_BRIGHT_BLUE,
};

#endif /* COLOR_H */
