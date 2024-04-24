//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//
// fxlink.tui.layout: Utility for laying out TUI windows in a flexbox treee
//
// This module can be used to set up a flexbox tree for window layout in the
// TUI. A `struct fxlink_TUI_box` is either an ncurses window, or a flexbox
// which arranges its children horizontally or vertically.
//
// Each box has some geometry settings:
// - w and h are initially set to the natural content size (can be 0)
// - min_w, min_h, max_w, max_h constrain the range of acceptable sizes
// - stretch_x and stretch_y indicate the box's tendency to grow
// - strech_force allows stretching beyond max_w/max_h (rarely needed)
//
// Most of the module is free-standing. Boxes should be created bottom to top,
// ie. windows first and then progressively larger groups. Specifying a WINDOW
// pointer in windows is optional; it is only used by fxlink_TUI_apply_layout()
// to actually configure ncurses windows. The root box of the tree should be
// kept in memory as it is used for rendering windows borders on the background
// window.
//
// Space distribution is initiated by a call to box_layout_root() after all the
// boxes have been created to form the tree. The root box receives the provided
// screen space, and splits it recursively between children. Boxes get their
// requested content size (clamped to minimum/maximum size) and any space left
// is distributed in proportion with stretch factors. Overflows are possible if
// the screen is too small to accommodate for everyone's content size, so the
// application of the layout should account for that and clamp to the screen
// size again (fxlink_TUI_apply_layout() does that).
//---

#pragma once
#include <fxlink/defs.h>
#include <ncurses.h>
#include <stdio.h>

#define FXLINK_TUI_BOX_MAXSIZE 8

/* Box either holding an ncurses WINDOW or arranging children in a flexbox. */
struct fxlink_TUI_box {
    /* Position and size, excluding the border (which is shared). After layout,
       x/y is the absolute position and w/h the allocated size. Other geometric
       settings are only relevant during layout. */
    uint16_t x, y, w, h;
    /* Size constraints */
    uint16_t min_w, min_h;
    uint16_t max_w, max_h;
    /* Stretch factor */
    uint8_t stretch_x, stretch_y;
    /* Stretch beyond limits */
    bool stretch_force;
    /* Box subdivision type: BOX_WINDOW, BOX_HORIZ and BOX_VERT */
    short type;

    union {
        /* Valid for type == FXLINK_TUI_BOX_WINDOW */
        struct {
            char const *title;
            WINDOW **win;
        } window;
        /* Valid for type == FXLINK_TUI_BOX_{HORIZONTAL,VERTICAL} */
        struct fxlink_TUI_box *children[FXLINK_TUI_BOX_MAXSIZE];
    };
};

enum {
    /* Box is an ncurses windows. Before layout, the natural size of the
       content is set in w and h (default 0), size constraints are set in
       {min,max}_{w,h} (default 0/65535) and stretch rates in stretch_x,
       stretch_y (default 1) and strech_force (default false). */
    FXLINK_TUI_BOX_WINDOW,
    /* Box is a horizontal of vertical flexbox. Before layout, children are
       specified and they induce a natural content size. Size constraints are
       stretch rates are specified as for windows. */
    FXLINK_TUI_BOX_HORIZONTAL,
    FXLINK_TUI_BOX_VERTICAL,
};

/* Make a window box. The title is used for the border rendering function in
   the TUI rendering utils. The window pointer is optional and only needed for
   fxlink_TUI_apply_layout(). */
struct fxlink_TUI_box *fxlink_TUI_box_mk_window(char const *title, WINDOW **w);
/* Make a vertical box with a fixed list of children */
struct fxlink_TUI_box *fxlink_TUI_box_mk_vertical(
    struct fxlink_TUI_box *child1, ... /*, NULL */);
/* Make a horizontal box with a fixed list of children */
struct fxlink_TUI_box *fxlink_TUI_box_mk_horizontal(
    struct fxlink_TUI_box *child1, ... /*, NULL */);

/* Specify the minimum size, maximum size and stretch rate of a box */
void fxlink_TUI_box_minsize(struct fxlink_TUI_box *box, int min_w, int min_h);
void fxlink_TUI_box_maxsize(struct fxlink_TUI_box *box, int max_w, int max_h);
void fxlink_TUI_box_stretch(struct fxlink_TUI_box *box,
    int stretch_x, int stretch_y, bool force);

/* Recursively print box and children starting at specified indent level */
void fxlink_TUI_box_print(FILE *fp, struct fxlink_TUI_box const *b, int level);

/* Layout a root box for the specified available screen space. This accounts
   for 1-unit borders around and between windows. For a full-screen window tree
   x/y would be set to 0 and w/h to the screen size, but the tree can also be
   laid out to occupy only a subset of screen space. */
void fxlink_TUI_box_layout(struct fxlink_TUI_box *root_box,
    int x, int y, int w, int h);

/* Recursively apply the layout. This function resizes and moves ncurses
   windows to fit the space allocated in the boxes. Returns false if an ncurses
   error causes one of the windows to become NULL. */
bool fxlink_TUI_apply_layout(struct fxlink_TUI_box *root_box);

