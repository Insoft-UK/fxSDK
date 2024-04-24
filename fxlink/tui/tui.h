//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//
// fxlink.tui.tui: Global data and main functions for the interactive TUI

#pragma once
#include <fxlink/tui/layout.h>
#include <fxlink/tui/render.h>
#include <fxlink/tui/input.h>
#include <fxlink/devices.h>
#include <fxlink/logging.h>

#include <libusb.h>
#include <ncurses.h>

struct TUIData {
    /* libusb context */
    libusb_context *ctx;
    /* SIGWINCH flag */
    bool resize_needed;
    /* ncurses window panels */
    WINDOW *wStatus;
    WINDOW *wLogs;
    WINDOW *wTransfers;
    WINDOW *wTextOutput;
    WINDOW *wConsole;
    /* Root box */
    struct fxlink_TUI_box *bRoot;
    /* Application data */
    struct fxlink_pollfds polled_fds;
    struct fxlink_device_list devices;
    /* Main console input */
    struct fxlink_TUI_input input;
};

extern struct TUIData TUI;

/* Run a single asynchronous update. This polls a bunch of file descriptors
   along with a short timeout (< 1s). Returns true if there is any activity.

   If `allow_console` is true, console events are handled; otherwise they are
   ignored so they can be collected by the main loop. Setting this parameter to
   false is useful when waiting for messages in TUI commands. `has_command` is
   set to whether there is a new command to be run at the console (only ever
   true when `allow_console` is true).

   If `auto_refresh` is true, this function will refresh the TUI upon relevant
   activity. */
bool TUI_core_update(bool allow_console, bool auto_refresh, bool *has_command);

/* Run the specified TUI command. */
void TUI_execute_command(char const *command);

/* Wait for a message of a particular type to arrive, and then clean it up.
   This function should be called in a loop, eg.

     struct fxlink_message *msg = NULL;
     while(TUI_wait_message(fdev, "fxlink", "text", &msg)) {
        // Handle msg...
     }

   TUI_wait_message() will only return true once, however it will use the next
   call to free the message, restart communication on the device, and reset
   `msg` to NULL. */
bool TUI_wait_message(struct fxlink_device *fdev,
    char const *application, char const *type, struct fxlink_message **msg);
