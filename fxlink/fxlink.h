//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//
// fxlink.fxlink: Options and mode functions

#pragma once
#include <fxlink/filter.h>
#include <libusb.h>

/* Global and command-line options. */
struct fxlink_options {
	/* If not NULL, gets a copy of all text messages received in either
	   interactive mode */
	FILE *log_file;
	/* Extra details (mainly interactive messages) */
	bool verbose;
};

extern struct fxlink_options options;

/* Main function for -l */
int main_list(struct fxlink_filter *filter, delay_t *delay,
	libusb_context *context);

/* Main function for -b */
int main_blocks(struct fxlink_filter *filter, delay_t *delay);

/* Main function for -s */
int main_send(struct fxlink_filter *filter, delay_t *delay, char **files);

/* Main function for -i */
int main_interactive(struct fxlink_filter *filter, delay_t *delay,
	libusb_context *context);

/* Main function for -t */
int main_tui_interactive(libusb_context *context);

/* Main function for -p */
int main_push(struct fxlink_filter *filter, delay_t *delay,
	libusb_context *context, char **files);
