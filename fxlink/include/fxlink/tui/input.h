//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//
// fxlink.tui.input: Asynchronous readline-style console input
//
// This header provides a basic asynchronous line edition mechanic attached to
// an ncurses window. It might be possible to use readline directly, but this
// is also a good exercise.
//---

#pragma once
#include <fxlink/defs.h>
#include <ncurses.h>

/* Input field attached to a window. This object only needs to be instantiated
   once for multiple inputs. */
struct fxlink_TUI_input {
    /* Line contents, NUL-terminated. The buffer might be larger. */
    char *data;
    /* Size of contents (not counting the NUL) */
    int size;
    /* Allocated size (always ≥ size+1) */
    int alloc_size;
    /* Cursor position within string */
    int cursor;
    /* Attached ncurses window */
    WINDOW *win;
    /* Original cursor position within window at start of input */
    uint16_t wx, wy;
};

//---
// Text manipulation functions
//---

/* Initialize the input at least init_chars characters of content available.
   Returns false on error. Previous contents are not freed! */
bool fxlink_TUI_input_init(struct fxlink_TUI_input *in, WINDOW *win,
   int init_chars);

/* Clean up a line and free its contents. */
void fxlink_TUI_input_free(struct fxlink_TUI_input *in);

/* Realloc the line to ensure n characters plus a NUL can be written. */
bool fxlink_TUI_input_alloc(struct fxlink_TUI_input *in, int n);

/* Insert n characters at position p. */
bool fxlink_TUI_input_insert(struct fxlink_TUI_input *in, int p,
   char const *str, int n);

/* Remove n characters at position p. Returns the number of characters
   actually removed after bounds checking. */
int fxlink_TUI_input_delete(struct fxlink_TUI_input *in, int p, int n);

//--
// Rendering functions
//---

/* Clear the input up to the original cursor position */
void fxlink_TUI_input_clear(struct fxlink_TUI_input *in);

/* Redraw the input (needed after non-appending edits) */
void fxlink_TUI_input_redraw(struct fxlink_TUI_input *in);

/* Clear the screen as with C-l */
void fxlink_TUI_input_clearscreen(struct fxlink_TUI_input *in);

/* getch() for an input (usually called when there *is* input */
bool fxlink_TUI_input_getch(struct fxlink_TUI_input *in, WINDOW *logWindow);
