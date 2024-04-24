//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//

#include "../fxlink.h"
#include "tui.h"
#include <fxlink/tooling/libpng.h>
#include <fxlink/tooling/sdl2.h>

#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

/*
Plan for the TUI command set.

fxlink commands:
  select                    Select the calculator we're talking to
  remote-control            Enable/disable/setup remote calculator control

Protocol commands (executed by the calculator):
  /identify                 Send calculator identification information
  /echo                     Echo a message
  /screenshot               Take a screenshot
  /video                    Enable/disable video capture

Application-specific commands:
  gintctl read-long         Transmission tests (long messages)
  gintctl read-alignment    Pipe reading alignment
  gintctl bench             USB transfer speed benchmark
*/

struct TUIData TUI = { 0 };

//---
// TUI management and rendering
//---

static bool TUI_setup_windows(void)
{
    struct fxlink_TUI_box
        *bTransfers, *bConsole, *bStatus, *bLogs, *bTextOutput,
        *bLeft, *bRight;

    bTransfers = fxlink_TUI_box_mk_window("Transfers", &TUI.wTransfers);
    bConsole = fxlink_TUI_box_mk_window("Console", &TUI.wConsole);
    bStatus = fxlink_TUI_box_mk_window("Status", &TUI.wStatus);
    bLogs = fxlink_TUI_box_mk_window("Logs", &TUI.wLogs);
    bTextOutput = fxlink_TUI_box_mk_window("Text output from calculators",
        &TUI.wTextOutput);
    fxlink_TUI_box_stretch(bLogs, 1, 2, false);

    bLeft = fxlink_TUI_box_mk_vertical(bTextOutput, bConsole, NULL);
    bRight = fxlink_TUI_box_mk_vertical(bStatus, bLogs, bTransfers, NULL);
    fxlink_TUI_box_stretch(bLeft, 2, 1, false);
    fxlink_TUI_box_stretch(bRight, 3, 1, false);

    TUI.bRoot = fxlink_TUI_box_mk_horizontal(bLeft, bRight, NULL);
    fxlink_TUI_box_layout(TUI.bRoot, 0, 1, getmaxx(stdscr), getmaxy(stdscr)-1);
    return fxlink_TUI_apply_layout(TUI.bRoot);
}

static void TUI_free_windows(void)
{
    if(TUI.wStatus)      delwin(TUI.wStatus);
    if(TUI.wLogs)        delwin(TUI.wLogs);
    if(TUI.wTransfers)   delwin(TUI.wTransfers);
    if(TUI.wTextOutput)  delwin(TUI.wTextOutput);
    if(TUI.wConsole)     delwin(TUI.wConsole);
}

static void TUI_refresh_all(bool refresh_bg)
{
    if(refresh_bg)
        wrefresh(stdscr);
    wrefresh(TUI.wStatus);
    wrefresh(TUI.wLogs);
    wrefresh(TUI.wTransfers);
    wrefresh(TUI.wTextOutput);
    wrefresh(TUI.wConsole);
}

static void TUI_refresh_console(void)
{
    wrefresh(TUI.wLogs);
    wrefresh(TUI.wConsole);
}

static void TUI_render_status(void)
{
    WINDOW *win = TUI.wStatus;
    werase(win);

    int w, h, y = 1;
    getmaxyx(win, h, w);

    wmove(win, 0, 1);
    fprint(win, FMT_HEADER,
        "Device   Status     ID         System    Classes");

    if(TUI.devices.count == 0) {
        mvwaddstr(win, 1, 1, "(no devices)");
        y++;
    }
    for(int i = 0; i < TUI.devices.count; i++) {
        struct fxlink_device *fdev = &TUI.devices.devices[i];
        struct fxlink_calc *calc = fdev->calc;

        if(fdev->status == FXLINK_FDEV_STATUS_CONNECTED) {
            wattron(win, fmt_to_ncurses_attr(FMT_BGSELECTED));
            mvwhline(win, y, 0, ' ', w);
        }

        mvwaddstr(win, y, 1, fxlink_device_id(fdev));
        mvwaddstr(win, y, 10, fxlink_device_status_string(fdev));
        mvwprintw(win, y, 21, "%04x:%04x", fdev->idVendor, fdev->idProduct);

        if(calc) {
            mvwaddstr(win, y, 32, fxlink_device_system_string(fdev));
            wmove(win, y, 41);

            for(int i = 0; i < calc->interface_count; i++)
                wprintw(win, " %02x.%02x", calc->classes[i] >> 8,
                    calc->classes[i] & 0xff);
        }

        wattroff(win, fmt_to_ncurses_attr(FMT_BGSELECTED));
        y++;
    }

    bool has_comms = false;
    for(int i = 0; i < TUI.devices.count; i++)
        has_comms = has_comms || (TUI.devices.devices[i].comm != NULL);

    wmove(win, y+1, 1);
    fprint(win, FMT_HEADER, "Device   Status     Serial    IN   OUT");
    y += 2;

    if(!has_comms) {
        mvwaddstr(win, y, 1, "(no communications)");
    }
    for(int i = 0; i < TUI.devices.count; i++) {
        struct fxlink_device *fdev = &TUI.devices.devices[i];
        struct fxlink_calc *calc = fdev->calc;
        struct fxlink_comm *comm = fdev->comm;
        if(!comm)
            continue;

        if(y >= h) {
            y++;
            break;
        }

        mvwaddstr(win, y, 1, fxlink_device_id(fdev));
        mvwaddstr(win, y, 10, fxlink_device_status_string(fdev));
        mvwaddstr(win, y, 21, calc->serial ? calc->serial : "(null)");
        mvwprintw(win, y, 31, "%02x%c", comm->ep_bulk_IN,
            comm->tr_bulk_IN != NULL ? '*' : '.');
        mvwprintw(win, y, 36, "%02x%c", comm->ep_bulk_OUT,
            comm->tr_bulk_OUT != NULL ? '*' : '.');
        y++;
    }

    if(y > h) {
        wmove(win, h-1, w-6);
        fprint(win, FMT_BGSELECTED, "(more)");
    }
}

static void progress_bar(WINDOW *win, int width, int done, int total)
{
    char const *ramp[9] = { " ", "▏", "▎", "▍", "▌", "▋", "▊", "▉", "█" };
    int progress = ((int64_t)done * width * 8) / total;
    int percent = (int64_t)done * 100 / total;

    wprintw(win, "%3d%% │", percent);
    for(int i = 0; i < width; i++) {
        int block_width = min(progress, 8);
        waddstr(win, ramp[block_width]);
        progress = max(progress-8, 0);
    }
    waddstr(win, "│");
}

static void TUI_render_transfers(void)
{
    WINDOW *win = TUI.wTransfers;
    int y = 1;

    werase(win);
    wmove(win, 0, 1);
    fprint(win, FMT_HEADER, "Device   Dir.  Size      Progress");
    bool has_transfers = false;

    for(int i = 0; i < TUI.devices.count; i++) {
        struct fxlink_device *fdev = &TUI.devices.devices[i];
        struct fxlink_comm *comm = fdev->comm;
        if(!comm)
            continue;

        struct fxlink_transfer *IN = comm->ftransfer_IN;
        struct fxlink_transfer *OUT = comm->ftransfer_OUT;

        if(IN) {
            mvwaddstr(win, y, 1, fxlink_device_id(fdev));
            mvwaddstr(win, y, 10, "IN");
            mvwaddstr(win, y, 16, fxlink_size_string(IN->msg.size));

            wmove(win, y, 26);
            progress_bar(win, 32, IN->processed_size, IN->msg.size);

            has_transfers = true;
            y++;
        }
        if(OUT) {
            mvwaddstr(win, y, 1, fxlink_device_id(fdev));
            mvwaddstr(win, y, 10, "OUT");
            mvwaddstr(win, y, 16, fxlink_size_string(OUT->msg.size));
            has_transfers = true;
            y++;
        }
    }

    if(!has_transfers)
        mvwaddstr(win, 1, 1, "(no transfers)");
}

static void TUI_render_all(bool with_borders)
{
    if(with_borders) {
        erase();
        fxlink_TUI_render_borders(TUI.bRoot);
        fxlink_TUI_render_titles(TUI.bRoot);

        /* Title bar */
        int w = getmaxx(stdscr);
        attron(fmt_to_ncurses_attr(FMT_BGSELECTED));
        mvhline(0, 0, ' ', w);
        char const *str = "fxlink " FXLINK_VERSION " (TUI interactive mode)";
        mvaddstr(0, w/2 - strlen(str)/2, str);
        attroff(fmt_to_ncurses_attr(FMT_BGSELECTED));
    }
    TUI_render_status();
    TUI_render_transfers();
}

static void TUI_SIGWINCH_handler(int sig)
{
    (void)sig;
    TUI.resize_needed = true;
}

static bool TUI_setup(void)
{
    memset(&TUI, 0, sizeof TUI);

    /* Set up the SINGWINCH handler */
    struct sigaction WINCH;
    sigaction(SIGWINCH, NULL, &WINCH);
    WINCH.sa_handler = TUI_SIGWINCH_handler;
    sigaction(SIGWINCH, &WINCH, NULL);

    /* Initialize the main screen */
    initscr();
    start_color();
    use_default_colors();

    /* Set up our color pairs. These are FG/BG combinations from the FMT_*
       enumerated colors in <fxlink/defs.h>, ordered BG-major. */
    for(int bg = 0; bg < 9; bg++) {
        for(int fg = 0; fg < 9; fg++)
            init_pair(9*bg + fg, fg - 1, bg - 1);
    }

    if(!TUI_setup_windows())
        return false;

    /* Allow ncurses to scroll text-based windows*/
    scrollok(TUI.wConsole, 1);
    scrollok(TUI.wLogs, 1);
    scrollok(TUI.wTextOutput, 1);

    wmove(TUI.wConsole, 0, 0);
    wmove(TUI.wLogs, 0, 0);
    wmove(TUI.wTextOutput, 0, 0);

    /* Make getch() non-blocking (though it also doesn't interpret escape
       sequences anymore!) */
    cbreak();
    wtimeout(TUI.wLogs, 0);
    wtimeout(TUI.wTextOutput, 0);
    wtimeout(TUI.wConsole, 0);

    /* Disable echo so we can edit input as it's being typed */
    noecho();

    return true;
}

static void TUI_quit(void)
{
    TUI_free_windows();
    endwin();
}

//---
// Interactive TUI
//---

static void handle_image(struct fxlink_message *msg, char const *path)
{
    struct fxlink_message_image_header *img = msg->data;
    char *filename = fxlink_gen_file_name(path, msg->type, ".png");

    struct fxlink_message_image_raw *raw = fxlink_message_image_decode(msg);
    if(raw) {
        fxlink_libpng_save_raw(raw, filename);
        fxlink_message_image_raw_free(raw);
        log_("saved image (%dx%d, format=%d) to '%s'\n",
            img->width, img->height, img->pixel_format, filename);
    }
    free(filename);
}

static void handle_text(struct fxlink_message *msg)
{
    char const *str = msg->data;
    WINDOW *win = TUI.wTextOutput;

    if(options.verbose)
        waddstr(win, "------------------\n");
    waddnstr(win, str, msg->size);
    if(options.verbose) {
        if(str[msg->size - 1] != '\n')
            waddch(win, '\n');
        waddstr(win, "------------------\n");
    }
    if(options.verbose) {
        for(size_t i = 0; i < msg->size; i++) {
            print(win, " %02x", str[i]);
            if((i & 15) == 15 || i == msg->size - 1)
                print(win, "\n");
        }
    }

    if(options.log_file)
        fwrite(str, 1, msg->size, options.log_file);
}

static void handle_video(struct fxlink_message *msg)
{
    struct fxlink_message_image_raw *raw = fxlink_message_image_decode(msg);
    if(!raw)
        return;

    fxlink_sdl2_display_raw(raw);
    fxlink_message_image_raw_free(raw);
}

static void fxlink_interactive_handle_message(struct fxlink_message *msg)
{
    char const *path = ".";

    if(fxlink_message_is_fxlink_image(msg))
        return handle_image(msg, path);

    if(fxlink_message_is_fxlink_text(msg))
        return handle_text(msg);

    if(fxlink_message_is_fxlink_video(msg))
        return handle_video(msg);

    /* Default to saving to a blob */
    static char combined_type[48];
    sprintf(combined_type, "%.16s-%.16s", msg->application, msg->type);

    char *filename = fxlink_gen_file_name(path, combined_type, ".bin");
    FILE *fp = fopen(filename, "wb");
    if(!fp) {
        elog("could not save to '%s': %m\n", filename);
        return;
    }

    fwrite(msg->data, 1, msg->size, fp);
    fclose(fp);

    log_("saved blob to '%s'\n", filename);
    free(filename);
}

static void handle_fxlink_log(int display_fmt, char const *str)
{
    int attr = fmt_to_ncurses_attr(display_fmt);
    wattron(TUI.wLogs, attr);
    waddstr(TUI.wLogs, str);
    wattroff(TUI.wLogs, attr);
}

bool TUI_core_update(bool allow_console, bool auto_refresh, bool *has_command)
{
    struct timeval zero_tv = { 0 };
    struct timeval usb_timeout;
    struct pollfd stdinfd = { .fd = STDIN_FILENO, .events = POLLIN };

    int rc = libusb_get_next_timeout(TUI.ctx, &usb_timeout);
    int timeout = -1;
    if(rc > 0)
        timeout = usb_timeout.tv_sec * 1000 + usb_timeout.tv_usec / 1000;
    bool timeout_is_libusb = true;
    /* Time out at least every 100 ms so we can handle SDL events */
    if(timeout < 0 || timeout > 100) {
        timeout = 100;
        timeout_is_libusb = false;
    }

    if(has_command)
        *has_command = false;

    rc = fxlink_multipoll(timeout,
        &stdinfd, 1, TUI.polled_fds.fds, TUI.polled_fds.count, NULL);

    if(rc < 0 && errno != EINTR) {
        elog("poll: %s\n", strerror(errno));
        return false;
    }
    if(rc < 0 && errno == EINTR)
        return false;

    /* Handle SIGWINCH */
    if(TUI.resize_needed) {
        endwin();
        refresh();
        TUI_setup_windows();
        TUI.resize_needed = false;
        TUI_render_all(true);
        TUI_refresh_all(true);
        return false;
    }

    /* Determine which even source was activated */
    bool stdin_activity = (stdinfd.revents & POLLIN) != 0;
    bool usb_activity = false;
    for(int i = 0; i < TUI.polled_fds.count; i++)
        usb_activity |= (TUI.polled_fds.fds[i].revents != 0);

    /* Determine what to do. We update the console on stdin activity. We
       update libusb on USB activity or appropriate timeout. We update SDL
       events on any timeout. */
    bool update_console = stdin_activity;
    bool update_usb = usb_activity || (rc == 0 && timeout_is_libusb);
    bool update_sdl = (rc == 0);

    if(allow_console && update_console) {
        bool finished = fxlink_TUI_input_getch(&TUI.input, TUI.wLogs);
        TUI_refresh_console();
        if(has_command)
            *has_command = finished;
    }

    if(update_usb) {
        libusb_handle_events_timeout(TUI.ctx, &zero_tv);
        fxlink_device_list_refresh(&TUI.devices);

        for(int i = 0; i < TUI.devices.count; i++) {
            struct fxlink_device *fdev = &TUI.devices.devices[i];

            /* Check for devices ready to connect to */
            if(fxlink_device_ready_to_connect(fdev)
            && fxlink_device_has_fxlink_interface(fdev)) {
                if(fxlink_device_claim_fxlink(fdev))
                    fxlink_device_start_bulk_IN(fdev);
            }
        }
    }

    if(update_sdl) {
        fxlink_sdl2_handle_events();
    }

    bool refresh = update_console || update_usb;
    if(auto_refresh) {
        TUI_render_all(false);
        TUI_refresh_all(false);
    }
    return refresh;
}

bool TUI_wait_message(struct fxlink_device *fdev,
    char const *application, char const *type, struct fxlink_message **msg_ptr)
{
    if(*msg_ptr) {
        fxlink_message_free(*msg_ptr, true);
        fxlink_device_start_bulk_IN(fdev);
        return false;
    }

    while(1) {
        TUI_core_update(false, true, NULL);

        /* Check for new messages */
        struct fxlink_message *msg = fxlink_device_finish_bulk_IN(fdev);
        if(msg) {
            if(fxlink_message_is_apptype(msg, application, type)) {
                *msg_ptr = msg;
                return true;
            }
            else {
                fxlink_interactive_handle_message(msg);
                fxlink_message_free(msg, true);
                fxlink_device_start_bulk_IN(fdev);
            }
        }
    }
}

int main_tui_interactive(libusb_context *ctx)
{
    if(!TUI_setup())
        return elog("error: failed to setup ncurses TUI o(x_x)o\n");

    TUI.ctx = ctx;

    /* Redirect fxlink logs to the logging window in the TUI */
    fxlink_log_set_handler(handle_fxlink_log);
    /* Set up hotplug notification */
    fxlink_device_list_track(&TUI.devices, ctx);
    /* Set up file descriptor tracking */
    fxlink_pollfds_track(&TUI.polled_fds, ctx);

    /* Initial render */
    print(TUI.wConsole, "fxlink version %s (libusb/TUI interactive mode)\n",
        FXLINK_VERSION);
    char const *prompt = "> ";

    print(TUI.wConsole, "%s", prompt);
    TUI_render_all(true);
    TUI_refresh_all(true);

    fxlink_TUI_input_init(&TUI.input, TUI.wConsole, 16);

    while(1) {
        bool has_command;
        bool activity = TUI_core_update(true, false, &has_command);

        /* Check for devices with finished transfers */
        for(int i = 0; i < TUI.devices.count; i++) {
            struct fxlink_device *fdev = &TUI.devices.devices[i];
            struct fxlink_message *msg = fxlink_device_finish_bulk_IN(fdev);
            if(msg) {
                fxlink_interactive_handle_message(msg);
                fxlink_message_free(msg, true);
                fxlink_device_start_bulk_IN(fdev);
            }
        }

        /* Check for console commands */
        if(has_command) {
            char *command = TUI.input.data;

            if(command[0] != 0)
                log_("command: '%s'\n", command);
            if(!strcmp(command, ""))
                {}
            else if(!strcmp(command, "q") || !strcmp(command, "quit"))
                break;
            else
                TUI_execute_command(command);

            fxlink_TUI_input_free(&TUI.input);
            print(TUI.wConsole, "%s", prompt);
            fxlink_TUI_input_init(&TUI.input, TUI.wConsole, 16);
        }

        if(activity) {
            TUI_render_all(false);
            TUI_refresh_all(false);
        }
    }

    while(fxlink_device_list_interrupt(&TUI.devices))
        libusb_handle_events(ctx);

    fxlink_device_list_stop(&TUI.devices);
    fxlink_pollfds_stop(&TUI.polled_fds);
    fxlink_log_set_handler(NULL);
    TUI_quit();
    return 0;
}
