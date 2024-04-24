//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//

#include <fxlink/defs.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

char const *fmt_to_ANSI(int format)
{
    static char buf[64];
    int n = 0;

    strcpy(buf, "\e[0m");
    n += 4;

    int FG = fmt_FG(format);
    int BG = fmt_BG(format);

    if(FG != 0)
        n += sprintf(buf+n, "\e[%dm", 30 + FG - 1);
    if(BG != 0)
        n += sprintf(buf+n, "\e[%dm", 40 + BG - 1);
    if(fmt_BOLD(format))
        strcpy(buf+n, "\e[1m"), n += 4;
    if(fmt_DIM(format))
        strcpy(buf+n, "\e[2m"), n += 4;
    if(fmt_ITALIC(format))
        strcpy(buf+n, "\e[3m"), n += 4;

    return buf;
}

char *fxlink_gen_file_name(char const *path, char const *name,
    char const *suffix)
{
    char *filename = NULL;
    int counter = 1;

    time_t time_raw;
    struct tm time_bd;
    time(&time_raw);
    localtime_r(&time_raw, &time_bd);

    while(1) {
        asprintf(&filename, "%s/fxlink-%.16s-%04d.%02d.%02d-%02dh%02d-%d%s",
            path, name, time_bd.tm_year + 1900, time_bd.tm_mon + 1,
            time_bd.tm_mday, time_bd.tm_hour, time_bd.tm_min, counter, suffix);
        if(!filename)
            continue;

        /* Try to find a name for a file that doesn't exist */
        if(access(filename, F_OK) == -1)
            break;

        free(filename);
        counter++;
    }

    return filename;
}

int fxlink_multipoll(int timeout, struct pollfd *fds1, int count1, ...)
{
    /* Convenience macro to iterate on file descriptor arrays */
    #define FOREACH_FD_ARRAY(FDS, COUNT, COUNT_ACC, BODY) do {  \
        struct pollfd *FDS; int COUNT, COUNT_ACC; va_list args; \
        va_start(args, count1);                                 \
        for(FDS = fds1, COUNT = count1, COUNT_ACC = 0; FDS;     \
            COUNT_ACC += COUNT,                                 \
            FDS = va_arg(args, struct pollfd *),                \
            COUNT = va_arg(args, int)) { BODY }                 \
        va_end(args); } while(0)

    /* Determine total number of file descriptors to watch */
    int total = 0;
    FOREACH_FD_ARRAY(fds, count, count_acc, {
        total += count;
    });

    struct pollfd *concat = malloc(total * sizeof *concat);
    if(!concat)
        return -ENOMEM;

    /* Copy from individual arrays to full array */
    FOREACH_FD_ARRAY(fds, count, count_acc, {
        memcpy(concat + count_acc, fds, count * sizeof *fds);
    });

    int rc = poll(concat, total, timeout);

    /* Copy back from full array to individual arrays */
    FOREACH_FD_ARRAY(fds, count, count_acc, {
        memcpy(fds, concat + count_acc, count * sizeof *fds);
    });

    free(concat);
    return rc;
}

char const *fxlink_size_string(int bytes)
{
    static char str[32];

    if(bytes > 1000000)
        sprintf(str, "%d.%d MB", bytes / 1000000, (bytes % 1000000) / 100000);
    else if(bytes > 1000)
        sprintf(str, "%d.%d kB", bytes / 1000, (bytes % 1000) / 100);
    else
        sprintf(str, "%d B", bytes);

    return str;
}

delay_t delay_none(void)
{
    return 0;
}
delay_t delay_seconds(int seconds)
{
    return seconds * 4;
}
delay_t delay_infinite(void)
{
    return -1;
}

bool delay_cycle(delay_t *delay)
{
    if(*delay == 0) return true;

    struct timespec spec = { .tv_sec=0, .tv_nsec=250000000 };
    int rc;

    /* Account for interrupts in the nanosleep(2) call */
    struct timespec req = spec;
    do rc = nanosleep(&req, &req);
    while(rc == -1 && errno == EINTR);

    if(*delay > 0) (*delay)--;
    return false;
}
