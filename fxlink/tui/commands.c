//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//

#include "tui.h"
#include "command-util.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

//---
// Standard commands
//---

FXLINK_COMMAND("/echo", DEVICE(fdev), VARIADIC(argv))
{
    int l = 5, j = 5;
    for(int i = 0; argv[i]; i++)
        l += strlen(argv[i]) + 1;

    char *concat = malloc(l + 1);
    strcpy(concat, "echo ");
    for(int i = 0; argv[i]; i++) {
        strcpy(concat + j, argv[i]);
        j += strlen(argv[i]);
        concat[j++] = (argv[i+1] == NULL) ? '\n' : ' ';
    }
    concat[j] = '\0';

    fxlink_device_start_bulk_OUT(fdev,
        "fxlink", "command", concat, l, true);
    return 0;
}

FXLINK_COMMAND("/identify", DEVICE(fdev))
{
    fxlink_device_start_bulk_OUT(fdev,
        "fxlink", "command", "identify", 8, false);
    return 0;
}

//---
// gintctl commands
//---

static char const *lipsum =
  "When the war of the beasts brings about the world's end,\n"
  "The goddess descends from the sky.\n"
  "Wings of light and dark spread afar,\n"
  "She guides us to bliss, her gift everlasting.\n"
  "\n"
  "Infinite in mystery is the gift of the goddess.\n"
  "We seek it thus, and take to the sky.\n"
  "Ripples form on the water's surface;\n"
  "The wandering soul knows no rest.\n"
  "\n"
  "There is no hate, only joy,\n"
  "For you are beloved by the goddess.\n"
  "Hero of the dawn, healer of worlds,\n"
  "Dreams of the morrow hath the shattered soul.\n"
  "Pride is lost -- wings stripped away, the end is nigh.\n"
  "\n"
  "My friend, do you fly away now?\n"
  "To a world that abhors you and I?\n"
  "All that awaits you is a somber morrow\n"
  "No matter where the winds may blow.\n"
  "My friend, your desire\n"
  "Is the bringer of life, the gift of the goddess.\n"
  "Even if the morrow is barren of promises,\n"
  "Nothing shall forestall my return.\n"
  "\n"
  "My friend, the fates are cruel.\n"
  "There are no dreams, no honor remains.\n"
  "The arrow has left the bow of the goddess.\n"
  "My soul, corrupted by vengeance,\n"
  "Hath endured torment to find the end of the journey\n"
  "In my own salvation and your eternal slumber.\n"
  "Legend shall speak of sacrifice at world's end\n"
  "The wind sails over the water's surface, quietly, but surely.\n"
  "\n"
  "Even if the morrow is barren of promises,\n"
  "Nothing shall forestall my return.\n"
  "To become the dew that quenches the lands,\n"
  "To spare the sands, the seas, the skies,\n"
  "I offer thee this silent sacrifice.\n";

FXLINK_COMMAND("gintctl echo-bounds", DEVICE(fdev), INT(count))
{
    if(count < 0 || count > 8192) {
        fprint(TUI.wConsole, FMT_RED, "error: ");
        print(TUI.wConsole, "count should be 0..8192 (not %d)\n", count);
        return 1;
    }

    uint32_t *data = malloc(count * 4);
    for(int i = 0; i < count; i++)
        data[i] = i;
    fxlink_device_start_bulk_OUT(fdev,
        "gintctl", "echo-bounds", data, count * 4, true);
    return 0;
}

FXLINK_COMMAND("gintctl garbage", DEVICE(fdev), INT(count))
{
    if(count < 0 || count > 8192) {
        fprint(TUI.wConsole, FMT_RED, "error: ");
        print(TUI.wConsole, "count should be 0..8192 (not %d)\n", count);
        return 1;
    }

    uint32_t *data = malloc(count * 4);
    for(int i = 0; i < count; i++)
        data[i] = i + 0xdead0000;
    fxlink_device_start_bulk_OUT(fdev,
        "gintctl", "garbage", data, count * 4, true);
    return 0;
}

static void status(bool b, char const *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprint(TUI.wConsole, b ? FMT_GREEN : FMT_RED, b ? "<PASSED> ":"<FAILED> ");
    vw_printw(TUI.wConsole, fmt, args);
    va_end(args);
}

static void unit_echo(struct fxlink_device *fdev, char const *str,
    char const *description)
{
    char *echo = malloc(5 + strlen(str) + 1);
    strcpy(echo, "echo ");
    strcat(echo, str);

    fxlink_device_start_bulk_OUT(fdev,
        "fxlink", "command", echo, strlen(echo), true);

    struct fxlink_message *msg = NULL;
    while(TUI_wait_message(fdev, "fxlink", "text", &msg)) {
        bool success =
            msg->size == strlen(str)
            && !strncmp(msg->data, str, msg->size);
        if(description)
            status(success, "%s\n", description);
        else
            status(success, "echo of '%s': '%.*s' (%d)\n", str, msg->size,
                (char *)msg->data, msg->size);
    }
}

static void unit_echo_bounds(struct fxlink_device *fdev, int count)
{
    char reference[256];
    sprintf(reference, "first=%08x last=%08x total=%d B\n",
        0, count-1, 4*count);

    uint32_t *data = malloc(count * 4);
    for(int i = 0; i < count; i++)
        data[i] = i;
    fxlink_device_start_bulk_OUT(fdev,
        "gintctl", "echo-bounds", data, count * 4, true);

    struct fxlink_message *msg = NULL;
    while(TUI_wait_message(fdev, "fxlink", "text", &msg)) {
        bool success =
            msg->size == strlen(reference)
            && !strncmp(msg->data, reference, msg->size);

        status(success, "echo bounds %d B\n", count * 4);
    }
}

static void unit_read_unaligned(struct fxlink_device *fdev, char const *str,
    int kind)
{
    char *payload = malloc(strlen(str) + 2);
    sprintf(payload, "%c%s", kind, str);

    fxlink_device_start_bulk_OUT(fdev,
        "gintctl", "read-unaligned", payload, strlen(payload), true);

    struct fxlink_message *msg = NULL;
    while(TUI_wait_message(fdev, "fxlink", "text", &msg)) {
        bool success =
            msg->size == strlen(str)
            && !strncmp(msg->data, str, msg->size);
        if(strlen(str) < 20)
            status(success, "unaligned echo type '%c' of '%s'\n", kind, str);
        else
            status(success, "unaligned echo type '%c' of %d-byte string\n",
                kind, strlen(str));
    }
}

static void test_read_basic(struct fxlink_device *fdev)
{
    unit_echo(fdev, "123", NULL);
    unit_echo(fdev, "1234", NULL);
    unit_echo(fdev, "12345", NULL);
    unit_echo(fdev, "123456", NULL);
    unit_echo(fdev, lipsum, "echo of better lorem ipsum");
}

static void test_read_buffers(struct fxlink_device *fdev)
{
    /* 128 and 384 bytes -> less than a packet */
    unit_echo_bounds(fdev, 32);
    unit_echo_bounds(fdev, 96);
    /* 512 bytes -> exactly one packet */
    unit_echo_bounds(fdev, 128);
    unit_echo_bounds(fdev, 128);
    unit_echo_bounds(fdev, 128);
    /* 516 and 768 -> one packet and a short one */
    unit_echo_bounds(fdev, 129);
    unit_echo_bounds(fdev, 192);
    /* 1024 bytes -> exactly two packets */
    unit_echo_bounds(fdev, 256);
    /* 2044 bytes -> just shy of a full buffer */
    unit_echo_bounds(fdev, 511);
    /* 2048 bytes -> a full buffer */
    unit_echo_bounds(fdev, 512);
    unit_echo_bounds(fdev, 512);
    /* 2300 bytes -> more than a full buffer */
    unit_echo_bounds(fdev, 575);
    /* 6000 bytes -> non-integral number of full buffers but more than 2 */
    unit_echo_bounds(fdev, 1500);
    /* 8192 bytes -> "large" amount of full buffers */
    unit_echo_bounds(fdev, 2048);
}

static void test_read_unaligned(struct fxlink_device *fdev)
{
    char const *alpha = "aBcDeFgHiJkLmNoPqR";

    for(int i = 1; i <= 9; i++)
        unit_read_unaligned(fdev, alpha, '0' + i);
    unit_read_unaligned(fdev, alpha, 'i');
    unit_read_unaligned(fdev, alpha, 'r');

    for(int i = 1; i <= 9; i++)
        unit_read_unaligned(fdev, lipsum, '0' + i);
    unit_read_unaligned(fdev, lipsum, 'i');
    unit_read_unaligned(fdev, lipsum, 'r');
}

FXLINK_COMMAND("gintctl test read-basic", DEVICE(fdev))
{
    test_read_basic(fdev);
    return 0;
}

FXLINK_COMMAND("gintctl test read-buffers", DEVICE(fdev))
{
    test_read_buffers(fdev);
    return 0;
}

FXLINK_COMMAND("gintctl test read-unaligned", DEVICE(fdev))
{
    test_read_unaligned(fdev);
    return 0;
}

FXLINK_COMMAND("gintctl test all", DEVICE(fdev))
{
    test_read_basic(fdev);
    test_read_buffers(fdev);
    test_read_unaligned(fdev);
    return 0;
}
