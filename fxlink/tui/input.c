//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//

#include <fxlink/tui/input.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//---
// Text manipulation functions
//---

bool fxlink_TUI_input_init(struct fxlink_TUI_input *in, WINDOW *win,
    int prealloc_size)
{
    char *data = malloc(prealloc_size + 1);
    if(!data)
        return false;

    data[0] = 0;
    in->data = data;
    in->size = 0;
    in->alloc_size = prealloc_size + 1;
    in->cursor = 0;
    in->win = win;

    getyx(win, in->wy, in->wx);
    scrollok(win, false);

    return true;
}

void fxlink_TUI_input_free(struct fxlink_TUI_input *in)
{
    free(in->data);
    memset(in, 0, sizeof *in);
}

bool fxlink_TUI_input_alloc(struct fxlink_TUI_input *in, int n)
{
    if(in->alloc_size >= n + 1)
        return true;

    /* Always increase the size by at least 16 so we can insert many times in a
       row without worrying about excessive allocations. */
    int newsize = (in->alloc_size + 16 > n + 1) ? in->alloc_size + 16 : n + 1;
    char *newdata = realloc(in->data, newsize);
    if(!newdata)
        return false;

    in->data = newdata;
    in->alloc_size = newsize;
    return true;
}

bool fxlink_TUI_input_insert(struct fxlink_TUI_input *in, int p,
    char const *str, int n)
{
    if(p < 0 || p > in->size)
        return false;
    if(!fxlink_TUI_input_alloc(in, in->size + n))
        return false;

    /* Move the end of the string (plus the NUL) n bytes forward */
    memmove(in->data + p + n, in->data + p, in->size - p + 1);
    memcpy(in->data + p, str, n);
    in->size += n;
    return true;
}

int fxlink_TUI_input_delete(struct fxlink_TUI_input *in, int p, int n)
{
    if(n >= in->size - p)
        n = in->size - p;

    if(n < 0)
        return 0;

    /* Move the end of the string (plus the NUL) n bytes backwards */
    memmove(in->data + p, in->data + p + n, in->size - n - p + 1);
    in->size -= n;
    return n;
}

//--
// Rendering functions
//---

bool fxlink_TUI_input_echo(struct fxlink_TUI_input *in, chtype ch)
{
    int x, y, w, h;
    getyx(in->win, y, x);
    getmaxyx(in->win, h, w);

    bool needs_scroll = (x == w-1 && y == h-1);
    bool can_scroll = in->wy > 0;

    if(!needs_scroll) {
        waddch(in->win, ch);
        return true;
    }
    else if(can_scroll) {
        waddch(in->win, ch);
        scrollok(in->win, true);
        scroll(in->win);
        scrollok(in->win, false);
        wmove(in->win, y, 0);
        in->wy--;
        return true;
    }
    else {
        return false;
    }
}

void fxlink_TUI_input_clear(struct fxlink_TUI_input *in)
{
    int w, h;
    getmaxyx(in->win, h, w);

    /* Clear from the end of screen to the start of the input */
    int current_y = h - 1;

    while(current_y > in->wy) {
        mvwhline(in->win, current_y, 0, ' ', w);
        current_y--;
    }
    mvwhline(in->win, in->wy, in->wx, ' ', w - in->wx);
}

void fxlink_TUI_input_redraw(struct fxlink_TUI_input *in)
{
    fxlink_TUI_input_clear(in);

    int cursor_x, cursor_y;

    waddnstr(in->win, in->data, in->cursor);
    getyx(in->win, cursor_y, cursor_x);
    waddnstr(in->win, in->data + in->cursor, in->size - in->cursor);

    wmove(in->win, cursor_y, cursor_x);
}

void fxlink_TUI_input_clearscreen(struct fxlink_TUI_input *in)
{
    int current_y, current_x;
    getyx(in->win, current_y, current_x);

    scrollok(in->win, true);
    wscrl(in->win, in->wy);
    scrollok(in->win, false);

    wmove(in->win, current_y - in->wy, current_x);
    in->wy = 0;
}

struct CSI {
    int param1;
    int param2;
    char cmd;
    char record[32];
};

static struct CSI parse_CSI(WINDOW *win, WINDOW *logWindow)
{
    struct CSI csi = { 0 };
    int i = 0;
    size_t j = 0;

    csi.param1 = -1;
    csi.param2 = -1;

    while(!csi.cmd && j < sizeof csi.record) {
        chtype ch = wgetch(win);
        if((int)ch == ERR) {
            wprintw(logWindow, "error: invalid CSI escape: ");
            waddnstr(logWindow, csi.record, j);
            waddch(logWindow, '\n');
            return (struct CSI){ 0 };
        }

        int c = ch & A_CHARTEXT;
        if(isdigit(c)) {
            if(i == 0) {
                csi.param1 = csi.param1 * 10 + (c - '0');
            }
            else {
                csi.param2 = csi.param2 * 10 + (c - '0');
            }
        }
        else if(c == ';' && i == 0)
            i = 1;
        else if(strchr("ABCD", c))
            csi.cmd = c;
        else {
            wprintw(logWindow, "unknow CSI escape\n");
            return (struct CSI){ 0 };
        }
    }

    return csi;
}

bool fxlink_TUI_input_getch(struct fxlink_TUI_input *in, WINDOW *logWindow)
{
    chtype ch = wgetch(in->win);
    int c = ch & A_CHARTEXT;
    struct CSI csi = { 0 };

    /* Parse CSI escapes */
    if(c == '\e') {
        chtype next = wgetch(in->win);
        if((int)next == ERR)
            return false;

        int n = next & A_CHARTEXT;
        /* CSI */
        if(n == '[')
            csi = parse_CSI(in->win, logWindow);
        /* Scrolling (ignored) */
        else if(n == 'O' || n == 'P')
            wgetch(in->win);
        else
            wprintw(logWindow, "error: invalid escape start ^[%c\n", n);

        c = 0;
    }

    /* Event left after a SIGWINCH */
    if(ch == KEY_RESIZE)
        return false;

    /* <Backspace>: Remove last letter */
    if(c == 0x7f) {
        if(in->cursor > 0) {
            fxlink_TUI_input_delete(in, in->cursor - 1, 1);
            in->cursor--;
            fxlink_TUI_input_redraw(in);
        }
    }
    /* C-d: Delete to the right */
    else if(c == 'D'-64) {
        if(in->cursor < in->size) {
            fxlink_TUI_input_delete(in, in->cursor, 1);
            fxlink_TUI_input_redraw(in);
        }
    }
    /* C-b: Back one character */
    else if(c == 'B'-64) {
        if(in->cursor > 0) {
            in->cursor--;
            fxlink_TUI_input_redraw(in);
        }
    }
    /* C-f: Forward one character */
    else if(c == 'F'-64) {
        if(in->cursor < in->size) {
            in->cursor++;
            fxlink_TUI_input_redraw(in);
        }
    }
    /* C-a: Move cursor to start of line */
    else if(c == 'A'-64) {
        if(in->cursor != 0) {
            in->cursor = 0;
            fxlink_TUI_input_redraw(in);
        }
    }
    /* C-e: Move cursor to end of line */
    else if(c == 'E'-64) {
        if(in->cursor != in->size) {
            in->cursor = in->size;
            fxlink_TUI_input_redraw(in);
        }
    }
    /* C-l: Clear the screen (moves prompt to first line) */
    else if(c == 'L'-64) {
        fxlink_TUI_input_clearscreen(in);
    }
    /* C-k: Kill to end of line */
    else if(c == 'K'-64) {
        fxlink_TUI_input_delete(in, in->cursor, in->size - in->cursor);
        fxlink_TUI_input_redraw(in);
    }
    /* <LEFT>: Move cursor to the left */
    else if(csi.cmd == 'D') {
        int distance = (csi.param1 >= 0) ? csi.param1 : 1;
        if(in->cursor > 0) {
            in->cursor = max(0, in->cursor - distance);
            fxlink_TUI_input_redraw(in);
        }
    }
    /* <RIGHT>: Move cursor to the right */
    else if(csi.cmd == 'C') {
        int distance = (csi.param1 >= 0) ? csi.param1 : 1;
        if(in->cursor < in->size) {
            in->cursor = min(in->size, in->cursor + distance);
            fxlink_TUI_input_redraw(in);
        }
    }
    // <DEL>: Delete to the right
    // Meta-F: Move forward a word
    // Meta-B: Move backward a word
    // Meta-<DEL> or Meta-<D>: Kill to end of current word or eat next word
    // Ctrl-W: Kill to previous whitespace

    else if(c == '\n') {
        in->cursor = in->size;
        fxlink_TUI_input_redraw(in);
        scrollok(in->win, true);
        waddch(in->win, '\n');
        return true;
    }
    else if(c != 0) {
        if(fxlink_TUI_input_echo(in, ch)) {
            char c_char = c;
            fxlink_TUI_input_insert(in, in->cursor, &c_char, 1);
            in->cursor++;
            fxlink_TUI_input_redraw(in);
        }
    }
    else if(csi.cmd) {
        wprintw(logWindow, "unhandled escape: %c (%d,%d)\n",
            csi.cmd, csi.param1, csi.param2);
    }

    /* Debug input as it is being typed
    if(c != 0) {
        wprintw(logWindow, "ch:%04x [%.*s|%.*s]\n",
            ch, in->cursor, in->data,
            in->size - in->cursor, in->data + in->cursor);
    } */

    return false;
}
