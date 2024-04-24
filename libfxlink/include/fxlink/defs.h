//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//
// fxlink.defs: Utility definitions and functions

#pragma once
#include <fxlink/config.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <poll.h>

static inline int min(int x, int y)
{
    return (x < y) ? x : y;
}
static inline int max(int x, int y)
{
    return (x > y) ? x : y;
}
static inline int clamp(int x, int min, int max)
{
    return (x < min) ? min : (x > max) ? max : x;
}

//---
// Text formatting
//
// We stick to ANSI style and provide 8 colors (foreground and background) with
// bold/italic/dim attributes. The standard ANSI escape translation is provided
// here while the ncurses versions is implemented by the TUI.
//---

enum {
    /* Main colors */
    FMT_BLACK     = 0x01,
    FMT_RED       = 0x02,
    FMT_GREEN     = 0x03,
    FMT_YELLOW    = 0x04,
    FMT_BLUE      = 0x05,
    FMT_MAGENTA   = 0x06,
    FMT_CYAN      = 0x07,
    FMT_WHITE     = 0x08,
    /* Background colors */
    FMT_BGBLACK   = 0x10,
    FMT_BGRED     = 0x20,
    FMT_BGGREEN   = 0x30,
    FMT_BGYELLOW  = 0x40,
    FMT_BGBLUE    = 0x50,
    FMT_BGMAGENTA = 0x60,
    FMT_BGCYAN    = 0x70,
    FMT_BGWHITE   = 0x80,
    /* Modifiers */
    FMT_BOLD      = 0x100,
    FMT_DIM       = 0x200,
    FMT_ITALIC    = 0x400,
};

#define fmt_FG(fmt) ((fmt) & 0xf)
#define fmt_BG(fmt) (((fmt) >> 4) & 0xf)
#define fmt_BOLD(fmt) (((fmt) & FMT_BOLD) != 0)
#define fmt_DIM(fmt) (((fmt) & FMT_DIM) != 0)
#define fmt_ITALIC(fmt) (((fmt) & FMT_ITALIC) != 0)

/* Returns the escape sequence that switches to the desired format. The
   returned pointer is to a static buffer overwritten on the next call. */
char const *fmt_to_ANSI(int format);

//---
// Misc.
//---

/* Generates a unique name for a file to be stored in `path`, with `name` as a
   component and the provided `suffix`. The generated path looks like

     <path>/fxlink-<name>-2021.05.09-19h23-1<suffix>

   with the `-1` suffix being chosen as to avoid overriding existing files.
   Returns a newly-allocated string to be free()'d after use. */
char *fxlink_gen_file_name(char const *path, char const *name,
    char const *suffix);

/* Modified poll with a variable number of arrays. */
int fxlink_multipoll(int timeout, struct pollfd *fds1, int count1, ...);

/* Write out the given size (in bytes) in a human-readable form. Returns a
   pointer to a statically-allocated string. */
char const *fxlink_size_string(int bytes);

//---
// Delay
//---

/* An expandable allocated time used to wait for devices */
typedef int delay_t;

/* Builds an empty delay. */
delay_t delay_none(void);
/* Builds a delay that lasts the specified number of seconds. */
delay_t delay_seconds(int seconds);
/* Builds an infinite delay. */
delay_t delay_infinite(void);

/* Returns `true` if the delay has expired; otherwise, waits for a short while
   (250 ms), decreases the supplied delay pointer, and returns `false`. Never
   returns `true` after waiting (even if the delay just expired) so the caller
   can attempt their task one last time before giving up on a timeout. */
bool delay_cycle(delay_t *delay);
