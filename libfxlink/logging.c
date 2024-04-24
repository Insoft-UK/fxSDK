//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//

#include <fxlink/logging.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

static fxlink_log_handler_t *log_handler = fxlink_log_stderr;

void fxlink_log_stderr(int display_fmt, char const *str)
{
    fputs(fmt_to_ANSI(display_fmt), stderr);
    fputs(str, stderr);
    fputs(fmt_to_ANSI(0), stderr);
}

void fxlink_log_set_handler(fxlink_log_handler_t *handler)
{
    log_handler = handler ? handler : fxlink_log_stderr;
}

static void handle_libusb_log(libusb_context *ctx, enum libusb_log_level level,
    char const *str)
{
    (void)ctx;
    (void)level;
    log_handler(0, str);
}

void fxlink_log_grab_libusb_logs(void)
{
    libusb_set_log_cb(NULL, handle_libusb_log, LIBUSB_LOG_CB_GLOBAL);
}

int flogv(int display_fmt, char const *fmt, va_list args)
{
    char *str = NULL;
    vasprintf(&str, fmt, args);
    if(str) {
        log_handler(display_fmt, str);
        free(str);
    }
    return 1;
}

#define LOG_VA_ARGS(FMT, STMTS) do { \
    va_list _args; \
    va_start(_args, FMT); \
    STMTS; \
    va_end(_args); \
} while(0)

int log_(char const *fmt, ...)
{
    LOG_VA_ARGS(fmt, flogv(0, fmt, _args));
    return 1;
}

int flog(int display_fmt, char const *fmt, ...)
{
    LOG_VA_ARGS(fmt, flogv(display_fmt, fmt, _args));
    return 1;
}

int elog(char const *fmt, ...)
{
    flog(FMT_RED, "error: ");
    LOG_VA_ARGS(fmt, flogv(0, fmt, _args));
    return 1;
}

int wlog(char const *fmt, ...)
{
    flog(FMT_YELLOW, "warning: ");
    LOG_VA_ARGS(fmt, flogv(0, fmt, _args));
    return 1;
}

int hlog(char const *fmt, ...)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);

    flog(FMT_WHITE | FMT_DIM, "[%02d:%02d] ", tm.tm_hour, tm.tm_min);
    LOG_VA_ARGS(fmt, flogv(FMT_YELLOW | FMT_DIM, fmt, _args));
    flog(FMT_YELLOW | FMT_DIM, ": ");
    return 1;
}
