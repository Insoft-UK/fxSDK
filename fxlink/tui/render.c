//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//

#include <fxlink/tui/render.h>
#include <time.h>

void aprint(WINDOW *win, int attr, char const *format, ...)
{
    va_list args;
    va_start(args, format);
    wattron(win, attr);
    vw_printw(win, format, args);
    wattroff(win, attr);
    va_end(args);
}

void fprint(WINDOW *win, int display_fmt, char const *format, ...)
{
    int attr = fmt_to_ncurses_attr(display_fmt);

    va_list args;
    va_start(args, format);
    wattron(win, attr);
    vw_printw(win, format, args);
    wattroff(win, attr);
    va_end(args);
}

int fmt_to_ncurses_attr(int format)
{
    /* Get the color pair for FG/BG following our custom encoding (which is
       compatible with the default for colors without a background) */
    int FG = fmt_FG(format);
    int BG = fmt_BG(format);
    int attr = COLOR_PAIR(9*BG + FG);

    if(fmt_BOLD(format))
        attr |= A_BOLD;
    if(fmt_DIM(format))
        attr |= A_DIM;
    if(fmt_ITALIC(format))
        attr |= A_ITALIC;

    return attr;
}

enum { TOP=1, RIGHT=2, BOTTOM=4, LEFT=8 };

static void TUI_add_borders(int x, int y, int directions)
{
    chtype borders[16] = {
        ' ',          '\0',        '\0',          ACS_LLCORNER,
        '\0',         ACS_VLINE,    ACS_ULCORNER, ACS_LTEE,
        '\0',         ACS_LRCORNER, ACS_HLINE,    ACS_BTEE,
        ACS_URCORNER, ACS_RTEE,     ACS_TTEE,     ACS_PLUS,
    };

    chtype ch = mvinch(y, x);

    int dirs = 0;
    for(int i = 0; i < 16; i++) {
        if(borders[i] == ch)
            dirs = i;
    }
    mvaddch(y, x, borders[dirs | directions]);
}

void fxlink_TUI_render_borders(struct fxlink_TUI_box const *b)
{
    int x = b->x, y = b->y;

    if(b->type == FXLINK_TUI_BOX_WINDOW) {
        for(int dx = 0; dx < b->w; dx++) {
            TUI_add_borders(x+dx, y-1, LEFT | RIGHT);
            TUI_add_borders(x+dx, y+b->h, LEFT | RIGHT);
        }
        for(int dy = 0; dy < b->h; dy++) {
            TUI_add_borders(x-1, y+dy, TOP | BOTTOM);
            TUI_add_borders(x+b->w, y+dy, TOP | BOTTOM);
        }

        TUI_add_borders(x-1, y-1, RIGHT | BOTTOM);
        TUI_add_borders(x+b->w, y-1, LEFT | BOTTOM);
        TUI_add_borders(x-1, y+b->h, RIGHT | TOP);
        TUI_add_borders(x+b->w, y+b->h, LEFT | TOP);
    }
    else {
        for(int i = 0; i < FXLINK_TUI_BOX_MAXSIZE && b->children[i]; i++)
            fxlink_TUI_render_borders(b->children[i]);
    }
}

void fxlink_TUI_render_titles(struct fxlink_TUI_box const *box)
{
    if(box->type == FXLINK_TUI_BOX_WINDOW) {
        attron(A_BOLD);
        mvaddch(box->y-1, box->x, ' ');
        addstr(box->window.title);
        addch(' ');
        attroff(A_BOLD);
        return;
    }
    for(int i = 0; i < FXLINK_TUI_BOX_MAXSIZE && box->children[i]; i++)
        fxlink_TUI_render_titles(box->children[i]);
}
