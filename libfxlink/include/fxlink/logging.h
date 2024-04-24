//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//
// fxlink.logging: Logging interface and log redirection
//
// This header provides basic logging utilities. You've seen these a thousand
// times over. The only features are supporting colors from <fxlink/defs.h>,
// redirecting libusb logs to this interface automatically, and redirecting
// this interface to either the terminal or the ncurses TUI.
//---

#pragma once
#include <fxlink/defs.h>
#include <libusb.h>
#include <stdio.h>

/* Type of the log handling function out of this interface */
typedef void fxlink_log_handler_t(int display_fmt, char const *str);

/* Default handler; prints to stderr with ANSI escapes. */
fxlink_log_handler_t fxlink_log_stderr;

/* Set the logging function. Default is fxlink_log_stderr. */
void fxlink_log_set_handler(fxlink_log_handler_t *handler);

/* Redirect libusb log to this module's log hander. */
void fxlink_log_grab_libusb_logs(void);

/* Log a message. All of the logging functions accept a printf()-style format
   with corresponding arguments, and return 1. */
int log_(char const *fmt, ...);
/* Log a message with a certain display format (color/bold/italic). */
int flog(int display_fmt, char const *fmt, ...);
/* Like log(), but adds a red "error:" in front. */
int elog(char const *fmt, ...);
/* Like log(), but adds a yellow "warning:" in front. */
int wlog(char const *fmt, ...);
/* Like log(), but for headers (with current time in gray in square brackets,
   and provided text in yellow followed by ":"). */
int hlog(char const *fmt, ...);

/* Warning that includes a libusb error message. */
#define wlog_libusb(RC, FMT, ...) \
    wlog(FMT ": %s\n", ##__VA_ARGS__, libusb_strerror(RC))
/* Error that includes a libusb error message. */
#define elog_libusb(RC, FMT, ...) \
    elog(FMT ": %s\n", ##__VA_ARGS__, libusb_strerror(RC))
