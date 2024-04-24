//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//
// fxlink.tui.render: TUI rendering utilities

#pragma once
#include <fxlink/tui/layout.h>
#include <ncurses.h>

/* printf to a window. */
#define print wprintw
/* printf with an ncurses attribute for the whole string. */
void aprint(WINDOW *win, int attr, char const *format, ...);
/* printf with an <fxlink/defs.h> format for the whole string. */
void fprint(WINDOW *win, int display_fmt, char const *format, ...);

/* Hard-coded color scheme */
#define FMT_FILENAME    FMT_CYAN
#define FMT_SIZE        FMT_MAGENTA
#define FMT_HEADER      (FMT_CYAN | FMT_ITALIC)
#define FMT_BGSELECTED  (FMT_BGWHITE | FMT_BLACK)

/* Translate <fxlink/defs.h> text format into ncurses attributes. */
int fmt_to_ncurses_attr(int display_fmt);

/* Recursively render borders around a box and its children. */
void fxlink_TUI_render_borders(struct fxlink_TUI_box const *box);

/* Render the window titles of all windows in the tree. */
void fxlink_TUI_render_titles(struct fxlink_TUI_box const *box);
