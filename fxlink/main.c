//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//

#include "fxlink.h"
#include <fxlink/filter.h>
#include <fxlink/logging.h>

#include <libusb.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <locale.h>

static const char *help_string =
"usage: %1$s (-l|-b|-t) [General options]\n"
"       %1$s -i [-r] [--fxlink-log[=<FILE>]] [General options]\n"
"       %1$s -p <FILE> [General options]\n"
"       %1$s -s <FILES>... [General options]\n"
"\n"
"fxlink interacts with CASIO calculators of the fx and fx-CG families over\n"
"the USB port, using libusb. It can also transfer files for Mass Storage\n"
"calculators using UDisks2.\n"
"\n"
"Standard (libusb) modes:\n"
"  -l, --list          List detected calculators on the USB ports\n"
"  -i, --interactive   Messaging with a gint add-in (calc -> PC only)\n"
"  -t, --tui           TUI interactive mode\n"
"  -p, --push          Push a .bin file to the Add-In Push app\n"
"\n"
"Mass Storage (UDisks2) modes:\n"
"  -b, --blocks        List detected Mass Storage calculators\n"
"  -s, --send          Send a file to a Mass Storage calc and unmount it\n"
"\n"
"General options:\n"
"  -v, --verbose       Increased verbosity (mostly in -i/-t)\n"
"  -w <SECONDS>        Wait this many seconds for a calculator to connect\n"
"  -w                  Wait indefinitely for a calculator to connect\n"
"  -f <FILTER>         Filter which calculators we connect to (see below)\n"
"  --libusb-log=LEVEL  libusb log level (NONE, ERROR, WARNING, INFO, DEBUG)\n"
"\n"
"Mode-specific options:\n"
"  --fxlink-log[=FILE] -i: Append fxlink text messages to FILE. Without\n"
"                          argument, a unique name is generated.\n"
"  -r, --repeat        -i: Reconnect if the calc disconnects (implies -w)\n"
"\n"
"Device filters:\n"
"  A device filter narrows down what devices we list or connect to by\n"
"  requiring a list of properties, such as \"p7,serial=00000001\". The\n"
"  following properties can be tested:\n"
"  - p7                Protocol 7 calcs (all fx models that use FA-124)\n"
"  - mass_storage      Mass Storage calcs (fx-CG models and the G-III)\n"
"  - series_cg         fx-CG models [udisks2 only]\n"
"  - series_g3         G-III models [udisks2 only]\n"
"  - serial=<SERIAL>   This serial number (needs write access in libusb)\n";

/* Global options */
struct fxlink_options options;

int main(int argc, char **argv)
{
	int rc=1, mode=0, error=0, option=0, loglevel=LIBUSB_LOG_LEVEL_WARNING;
	delay_t delay = delay_seconds(0);
	struct fxlink_filter *filter = NULL;
	bool repeat = false;

	options.log_file = NULL;
	options.verbose = false;

	setlocale(LC_ALL, "");

	//---
	// Command-line argument parsing
	//---

	enum { LIBUSB_LOG=1, LOG_TO_FILE=2 };
	const struct option longs[] = {
		{ "help",        no_argument,       NULL, 'h' },
		{ "list",        no_argument,       NULL, 'l' },
		{ "blocks",      no_argument,       NULL, 'b' },
		{ "send",        no_argument,       NULL, 's' },
		{ "interactive", no_argument,       NULL, 'i' },
		{ "tui",         no_argument,       NULL, 't' },
		{ "push",        no_argument,       NULL, 'p' },
		{ "libusb-log",  required_argument, NULL, LIBUSB_LOG },
		{ "fxlink-log",  optional_argument, NULL, LOG_TO_FILE },
		{ "repeat",      no_argument,       NULL, 'r' },
		{ "verbose",     no_argument,       NULL, 'v' },
		/* Deprecated options ignored for compatibility: */
		{ "quiet",       no_argument,       NULL, 'q' },
		{ "unmount",     no_argument,       NULL, 'u' },
		{ NULL },
	};

	while(option >= 0 && option != '?')
	switch((option = getopt_long(argc, argv, "hlbsitpquf:w::rv", longs, NULL)))
	{
	case 'h':
		fprintf(stderr, help_string, argv[0]);
		return 0;
	case 'l':
	case 'b':
	case 's':
	case 'i':
	case 't':
	case 'p':
		mode = option;
		break;
	case LIBUSB_LOG:
		if(!strcmp(optarg, "NONE"))
			loglevel = LIBUSB_LOG_LEVEL_NONE;
		else if(!strcmp(optarg, "ERROR"))
			loglevel = LIBUSB_LOG_LEVEL_ERROR;
		else if(!strcmp(optarg, "WARNING"))
			loglevel = LIBUSB_LOG_LEVEL_WARNING;
		else if(!strcmp(optarg, "INFO"))
			loglevel = LIBUSB_LOG_LEVEL_INFO;
		else if(!strcmp(optarg, "DEBUG"))
			loglevel = LIBUSB_LOG_LEVEL_DEBUG;
		else fprintf(stderr, "warning: ignoring log level '%s'; should be "
				"NONE, ERROR, WARNING, INFO or DEBUG\n", optarg);
		break;
	case 'q':
		/* Ignored: -q, --quiet used to control some messages but is now
		   supplanted by not setting -v */
		break;
	case 'u':
		/* Ignored: -u, --unmount used to force unmounting filesystems after -s
		   (which is now the only behavior) */
		break;
	case 'r':
		repeat = true;
		delay = delay_infinite();
		break;
	case 'v':
		options.verbose = true;
		break;
	case LOG_TO_FILE:
		if(optarg)
			options.log_file = fopen(optarg, "a");
		else {
			char *name = fxlink_gen_file_name(".", "logfile", ".log");
			printf("--fxlink-log will output in '%s'\n", name);
			options.log_file = fopen(name, "a");
			free(name);
		}
		break;
	case 'w':
		if(!optarg) {
			delay = delay_infinite();
			break;
		}
		char *end;
		int seconds = strtol(optarg, &end, 10);
		if(seconds < 0 || *end != 0) {
			error = elog("invalid delay '%s'\n", optarg);
			break;
		}
		delay = delay_seconds(seconds);
		break;
	case 'f':
		filter = fxlink_filter_parse(optarg);
		break;
	case '?':
		error = 1;
	}

	if(mode == 's' && optind == argc)
		error = elog("send mode requires additional arguments (file names)\n");

	if(mode == 'p' && optind == argc)
		error = elog("push mode requires a file name\n");
	if(mode == 'p' && optind < argc-1)
		error = elog("push mode only accepts one file name\n");

	/* No arguments or bad arguments */
	if(error)
		return 1;
	if(!mode) {
		fprintf(stderr, help_string, argv[0]);
		return 1;
	}

	/* Default filter */
	if(filter == NULL)
		filter = calloc(1, sizeof *filter);

	//---
	// libusb initialization
	//---

	libusb_context *context = NULL;

	/* Initialize libusb for corresponding modes */
	if(mode == 'l' || mode == 'i' || mode == 't' || mode == 'p') {
		if((rc = libusb_init(&context)))
			return elog_libusb(rc, "error initializing libusb");
		libusb_set_option(context, LIBUSB_OPTION_LOG_LEVEL, loglevel);
		fxlink_log_grab_libusb_logs();
	}

	//---
	// Main functions
	//---

	if(mode == 'l') {
		rc = main_list(filter, &delay, context);
	}
	else if(mode == 'b') {
		#ifndef FXLINK_DISABLE_UDISKS2
		rc = main_blocks(filter, &delay);
		#else
		rc = elog("this fxlink was built without UDisks2; -b is disabled");
		#endif
	}
	else if(mode == 's') {
		#ifndef FXLINK_DISABLE_UDISKS2
		rc = main_send(filter, &delay, argv + optind);
		#else
		rc = elog("this fxlink was built without UDisks2; -s is disabled");
		#endif
	}
	else if(mode == 'i') {
		do {
			rc = main_interactive(filter, &delay, context);
		}
		while(repeat);
	}
	else if(mode == 't') {
		rc = main_tui_interactive(context);
	}
	else if(mode == 'p') {
		rc = main_push(filter, &delay, context, argv + optind);
	}

	fxlink_filter_free(filter);
	if(context)
		libusb_exit(context);
	if(options.log_file)
		fclose(options.log_file);
	return rc;
}
