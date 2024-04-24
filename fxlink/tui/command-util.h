//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//
// fxlink.tui.command-util: Preprocessor black magic for command definition
//
// This header provides the following method for declaring TUI commands that
// are automatically registered at startup, and are invoked with arguments
// pre-parsed:
//
// FXLINK_COMMAND("<NAME>", <TYPE>(<NAME>), <TYPE>(<NAME>), ...) {
//    /* normal code... */
//    return <STATUS>;
// }
//
// The command name is a string. It can have multiple space-separated words as
// in "gintctl test", in which case it is matched against argv[0], argv[1], etc
// and the prefix ("gintctl") is automatically made into a sub-category command
// with relevant error messages.
//
// Each argument has a type and a name, as in INT(x). The type carries
// information on the parsing method, the acceptable range, and of course the
// actual runtime type of the argument. Available types are:
//
//   Name      Runtime type            Meaning and range
//   --------------------------------------------------------------------------
//   INT       int                     Any integer
//   STRING    char const *            Any string argument from argv[]
//   VARIADIC  char const **           End of the argv array (NULL-terminated)
//   --------------------------------------------------------------------------
//   DEVICE    struct fxlink_device *  Selected device (implicit; never NULL)
//   --------------------------------------------------------------------------
//
// The function returns a status code, which is an integer. The entire command
// declaration might look like:
//
// FXLINK_COMMAND("gintctl test", INT(lower_bound), INT(upper_bound)) {
//     int avg = (lower_bound + upper_bound) / 2;
//     return 0;
// }
//
// I considered doing the entire thing in C++, but absolute preprocessor abuse
// is fun once in a while.
//---

#include <fxlink/defs.h>

//---
// Shell-like command parsing (without the features)
//---

struct fxlink_tui_cmd {
    int argc;
    char const **argv;
    char *data;
};

/* Parse a string into an argument vector */
struct fxlink_tui_cmd fxlink_tui_cmd_parse(char const *input);

/* Dump a command to TUI console for debugging */
void fxlink_tui_cmd_dump(struct fxlink_tui_cmd const *cmd);

/* Free a command */
void fxlink_tui_cmd_free(struct fxlink_tui_cmd const *cmd);

//---
// Command registration and argument scanning
//---

/* Parse a list of arguments into structured data. The format is a string of
   argument specifiers, each of which can be:
     s   String (char *)
     d   Integer (int)
     *   Other variadic arguments (char **)
   (-- will probably be expanded later.)
   Returns true if parsing succeeded, false otherwise (including if arguments
   are missing) after printing an error message. */
bool fxlink_tui_parse_args(int argc, char const **argv, char const *fmt, ...);

/* Register a command with the specified name and invocation function. This can
   be called manually or generated (along with the parser) using the macro
   FXLINK_COMMAND. */
void fxlink_tui_register_cmd(char const *name,
    int (*func)(int argc, char const **argv));

/* Apply a macro to every variadic argument. _M1 is applied to the first
   argument and _Mn is applied to all subsequent arguments. */
#define MAPn(_M1,_Mn,...)             __VA_OPT__(MAP_1(_M1,_Mn,__VA_ARGS__))
#define MAP_1(_M1,_Mn,_X,...) _M1(_X) __VA_OPT__(MAP_2(_M1,_Mn,__VA_ARGS__))
#define MAP_2(_M1,_Mn,_X,...) _Mn(_X) __VA_OPT__(MAP_3(_M1,_Mn,__VA_ARGS__))
#define MAP_3(_M1,_Mn,_X,...) _Mn(_X) __VA_OPT__(MAP_4(_M1,_Mn,__VA_ARGS__))
#define MAP_4(_M1,_Mn,_X,...) _Mn(_X) __VA_OPT__(MAP_5(_M1,_Mn,__VA_ARGS__))
#define MAP_5(_M1,_Mn,_X,...) _Mn(_X) __VA_OPT__(MAP_6(_M1,_Mn,__VA_ARGS__))
#define MAP_6(_M1,_Mn,_X,...) _Mn(_X) __VA_OPT__(MAP_7(_M1,_Mn,__VA_ARGS__))
#define MAP_7(_M1,_Mn,_X,...) _Mn(_X) __VA_OPT__(MAP_8(_M1,_Mn,__VA_ARGS__))
#define MAP_8(_M1,_Mn,_X,...) _Mn(_X) __VA_OPT__(MAP_MAX_8_ARGS(_))
#define MAP_MAX_8_ARGS()
/* Simpler version where the same macro is applied to all arguments */
#define MAP(_M, ...) MAPn(_M, _M, ##__VA_ARGS__)

/* Command declaration macro. Builds an invocation function and a registration
   function so the command name doesn't have to be repeated. */
#define FXLINK_COMMAND(_NAME, ...) DO_COMMAND1(_NAME, __COUNTER__, __VA_ARGS__)
/* This call forces __COUNTER__ to expand */
#define DO_COMMAND1(...) DO_COMMAND(__VA_ARGS__)

#define DO_COMMAND(_NAME, _COUNTER, ...) \
    static int ___command_ ## _COUNTER(); \
    static int ___invoke_command_ ## _COUNTER \
            (int ___argc, char const **___argv) { \
        MAP(MKVAR, ##__VA_ARGS__) \
        if(!fxlink_tui_parse_args(___argc, ___argv, \
            "" MAP(MKFMT, ##__VA_ARGS__) \
            MAP(MKPTR, ##__VA_ARGS__))) return 1; \
        return ___command_ ## _COUNTER( \
            MAPn(MKCALL_1, MKCALL_n, ##__VA_ARGS__)); \
    } \
    __attribute__((constructor)) \
    static void ___declare_command_ ## _COUNTER (void) { \
        fxlink_tui_register_cmd(_NAME, ___invoke_command_ ## _COUNTER); \
    } \
    static int ___command_ ## _COUNTER(MAPn(MKFML_1, MKFML_n, ##__VA_ARGS__))

/* Make the format string for an argument */
#define MKFMT(_TV) MKFMT_ ## _TV
#define MKFMT_INT(_X) "i"
#define MKFMT_STRING(_X) "s"
#define MKFMT_VARIADIC(_X) "*"
#define MKFMT_DEVICE(_X) "d"

/* Make the formal function parameter for an argument */
#define MKFML_1(_TV) MKFML_ ## _TV
#define MKFML_n(_TV) , MKFML_1(_TV)
#define MKFML_INT(_X) int _X
#define MKFML_STRING(_X) char const * _X
#define MKFML_VARIADIC(_X) char const ** _X
#define MKFML_DEVICE(_X) struct fxlink_device * _X

/* Create a variable */
#define MKVAR(_TV) MKFML_1(_TV);

/* Make a pointer to an argument (sadly we can't get the name directly) */
#define MKPTR(_TV) , MKPTR_ ## _TV
#define MKPTR_INT(_X) &_X
#define MKPTR_STRING(_X) &_X
#define MKPTR_VARIADIC(_X) &_X
#define MKPTR_DEVICE(_X) &_X

/* Pass a variable as a function argument */
#define MKCALL_1(_TV) MKCALL_ ## _TV
#define MKCALL_n(_TV) , MKCALL_1(_TV)
#define MKCALL_INT(_X) _X
#define MKCALL_STRING(_X) _X
#define MKCALL_VARIADIC(_X) _X
#define MKCALL_DEVICE(_X) _X
