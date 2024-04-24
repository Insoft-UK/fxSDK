#include <stdio.h>
#include <string.h>
#include <time.h>
#include <getopt.h>

#include <fxgxa.h>
#include <g1a.h>
#include <g3a.h>

static const char *help_string =
"usage: fxgxa [-g] <binary file> [options...]\n"
"       fxgxa  -e  <g1a/g3a file> [options...]\n"
"       fxgxa  -d  <g1a/g3a file>\n"
"       fxgxa  -r  <g1a/g3a file> [-o <g1a/g3a file>]\n"
"       fxgxa  -x  <g1a/g3a file> [-o <png file>]\n"
"\n"
"fxgxa creates or edits g1a and g3a files (add-in applications for CASIO\n"
"fx-9860G and fx-CG series) that consist of a header followed by code.\n"
"\n"
"Operating modes:\n"
"  -g, --g1a, --g3a      Generate a g1a/g3a file (default)\n"
"  -e, --edit            Edit header of an existing g1a/g3a file\n"
"  -d, --dump            Dump header of an existing g1a/g3a file\n"
"  -r, --repair          Recalculate control bytes and checksums\n"
"  -x, --extract         Extract icon into a PNG file\n"
"\n"
"General options:\n"
"  -o, --output=<file>   Output file (default: input with .g1a/.g3a suffix\n"
"                        [-g]; with .png suffix [-x]; input file [-e, -r])\n"
"  --output-uns=<file>   Output for unselected icon with [-x] and g3a file\n"
"  --output-sel=<file>   Output for selected icon with [-x] and g3a file\n"
"\n"
"Generation and edition options:\n"
"  -i, --icon=<png>      Program icon, in PNG format (default: blank) [g1a]\n"
"  --icon-uns=<png>      Unselected program icon, in PNG format [g3a]\n"
"  --icon-sel=<png>      Selected program icon, in PNG format [g3a]\n"
"  -n, --name=<name>     Add-in name (default: output file name)\n"
"  --version=<text>      Program version, MM.mm.pppp format (default: empty)\n"
"  --internal=<name>     Internal name, eg. '@NAME' (default: empty)\n"
"  --date=<date>         Date of build, yyyy.MMdd.hhmm (default: now)\n";

/*
**  Field customization
*/

/* A set of user-defined fields, often taken on the command-line
   Default values are NULL and indicate "no value" (-g) or "no change" (-e). */
struct fields
{
	/* New values for basic fields */
	const char *name;
	const char *version;
	const char *internal;
	const char *date;
	/* Icon file name */
	const char *icon;
	const char *icon_uns, *icon_sel;

	// TODO: G3A: Fill the filename field
};

/* fields_edit(): Set the value of some fields altogether
   @gxa      Header to edit, is assumed checksumed and filled
   @fields   New values for fields, any members can be NULL */
void fields_edit(void *gxa, struct fields const *fields)
{
	/* For easy fields, just call the appropriate edition function */
	if(fields->name)     edit_name(gxa, fields->name);
	if(fields->version)  edit_version(gxa, fields->version);
	if(fields->internal) edit_internal(gxa, fields->internal);
	if(fields->date)     edit_date(gxa, fields->date);

	/* Load icon from PNG file */
	if(fields->icon && is_g1a(gxa)) {
		int w, h;
		uint8_t *rgb24 = icon_load(fields->icon, &w, &h);

		if(rgb24) {
			/* Skip the first row if h > 17, since the usual
			   representation at 30x19 skips the first and last */
			uint8_t *mono = icon_conv_24to1(
				h > 17 ? rgb24 + 3*w : rgb24, w, h - (h > 17));
			if(mono) edit_g1a_icon(gxa, mono);
			free(mono);
		}
		free(rgb24);
	}
	if(fields->icon_uns && is_g3a(gxa)) {
		int w, h;
		uint8_t *rgb24 = icon_load(fields->icon_uns, &w, &h);

		if(rgb24) {
			uint16_t *rgb16be = icon_conv_24to16(rgb24, w, h);
			if(rgb16be) edit_g3a_icon(gxa, rgb16be, false);
			free(rgb16be);
		}
		free(rgb24);
	}
	if(fields->icon_sel && is_g3a(gxa)) {
		int w, h;
		uint8_t *rgb24 = icon_load(fields->icon_sel, &w, &h);

		if(rgb24) {
			uint16_t *rgb16be = icon_conv_24to16(rgb24, w, h);
			if(rgb16be) edit_g3a_icon(gxa, rgb16be, true);
			free(rgb16be);
		}
		free(rgb24);
	}
}

/*
**  Tool implementation
*/

int main(int argc, char **argv)
{
	/* Result of option parsing */
	int mode = 'g', error = 0;
	struct fields fields = { 0 };
	const char *output = NULL;
	const char *output_uns=NULL, *output_sel=NULL;

	const struct option longs[] = {
		{ "help",       no_argument,       NULL, 'h' },
		{ "g1a",        no_argument,       NULL, '1' },
		{ "g3a",        no_argument,       NULL, '3' },
		{ "edit",       no_argument,       NULL, 'e' },
		{ "dump",       no_argument,       NULL, 'd' },
		{ "repair",     no_argument,       NULL, 'r' },
		{ "extract",    no_argument,       NULL, 'x' },
		{ "output",     required_argument, NULL, 'o' },
		{ "output-uns", required_argument, NULL, 'O' },
		{ "output-sel", required_argument, NULL, 'P' },
		{ "icon",       required_argument, NULL, 'i' },
		{ "icon-uns",   required_argument, NULL, 'I' },
		{ "icon-sel",   required_argument, NULL, 'J' },
		{ "name",       required_argument, NULL, 'n' },
		{ "version",    required_argument, NULL, 'v' },
		{ "internal",   required_argument, NULL, 't' },
		{ "date",       required_argument, NULL, 'a' },
		{ NULL,         0,                 NULL, 0   },
	};

	int option = 0;
	while(option >= 0 && option != '?')
	switch((option = getopt_long(argc, argv, "hgedrxo:i:n:", longs, NULL)))
	{
	case 'h':
		fprintf(stderr, help_string, argv[0]);
		return 0;
	case 'g':
	case 'e':
	case 'd':
	case 'r':
	case 'x':
	case '1':
	case '3':
		mode = option;
		break;
	case 'o':
		output = optarg;
		break;
	case 'O':
		output_uns = optarg;
		break;
	case 'P':
		output_sel = optarg;
		break;
	case 'i':
		fields.icon = optarg;
		break;
	case 'I':
		fields.icon_uns = optarg;
		break;
	case 'J':
		fields.icon_sel = optarg;
		break;
	case 'n':
		fields.name = optarg;
		break;
	case 'v':
		fields.version = optarg;
		break;
	case 't':
		fields.internal = optarg;
		break;
	case 'a':
		fields.date = optarg;
		break;
	case '?':
		error = 1;
		break;
	}

	if(mode == 'g' && !strcmp(argv[0], "fxg1a"))
		mode = '1';
	if(mode == 'g' && !strcmp(argv[0], "fxg3a"))
		mode = '3';
	if(error) return 1;

	if(argv[optind] == NULL)
	{
		fprintf(stderr, help_string, argv[0]);
		return 1;
	}
	if(mode == 'g') {
		fprintf(stderr, "cannot guess -g; use --g1a or --g3a\n");
		return 1;
	}
	if(mode == '1' || mode == '3')
	{
		/* Load binary file into memory */
		size_t size;
		int header = (mode == '1' ? 0x200 : 0x7000);
		int footer = (mode == '1' ? 0 : 4);
		void *gxa = load_binary(argv[optind], &size, header, footer);
		if(!gxa) return 1;

		/* If [output] is set, use it, otherwise compute a default */
		char *alloc = NULL;
		if(!output)
		{
			alloc = malloc(strlen(argv[optind]) + 5);
			if(!alloc) {fprintf(stderr, "error: %m\n"); return 1;}
			default_output(argv[optind],
				(mode == '1' ? ".g1a" : ".g3a"), alloc);
		}

		/* First set the type so that is_g1a() and is_g3a() work */
		((uint8_t *)gxa)[8] = (mode == '1' ? 0xf3 : 0x2c);

		/* Start with output file name as application name */
		edit_name(gxa, output ? output : alloc);

		/* Start with "now" as build date */
		char date[15];
		time_t t = time(NULL);
		struct tm *now = localtime(&t);
		strftime(date, 15, "%Y.%m%d.%H%M", now);
		edit_date(gxa, date);

		/* Start with an uppercase name as internal name */
		char internal[12];
		if(fields.name)
			default_internal(fields.name, internal, 11);
		else if(is_g1a(gxa))
			default_internal(G1A(gxa)->header.name, internal, 11);
		else if(is_g3a(gxa))
			default_internal(G3A(gxa)->header.name, internal, 11);
		edit_internal(gxa, internal);

		/* Edit the fields with user-customized values */
		fields_edit(gxa, &fields);

		/* Set fixed fields and calculate checksums */
		sign(gxa, size);

		save_gxa(output ? output : alloc, gxa, size);
		free(alloc);
		free(gxa);
	}
	if(mode == 'e')
	{
		/* Load file into memory */
		size_t size;
		void *gxa = load_gxa(argv[optind], &size);
		if(!gxa) return 1;

		/* Edit the fields with user-customized values */
		fields_edit(gxa, &fields);

		/* We don't reset fixed fields or recalculate checksums because
		   we only want to edit what was requested by the user.
		   Besides, the control bytes and checksums do *not* depend on
		   the value of user-customizable fields. */

		/* Regenerate input file, or output somewhere else */
		if(!output) output = argv[optind];
		save_gxa(output, gxa, size);
		free(gxa);
	}
	if(mode == 'd')
	{
		size_t size;
		void *gxa = load_gxa(argv[optind], &size);
		if(!gxa) return 1;

		dump(gxa, size);
		free(gxa);
	}
	if(mode == 'r')
	{
		size_t size;
		void *gxa = load_gxa(argv[optind], &size);
		if(!gxa) return 1;

		/* Repair file by recalculating fixed fields and checksums */
		sign(gxa, size);

		/* Regenerate input file, or output somewhere else */
		if(!output) output = argv[optind];
		save_gxa(output, gxa, size);
		free(gxa);
	}
	if(mode == 'x')
	{
		size_t size;
		void *gxa = load_gxa(argv[optind], &size);
		if(!gxa) return 1;

		if(is_g1a(gxa)) {
			/* Add clean top/bottom rows */
			uint8_t mono[76];
			memcpy(mono, "\x00\x00\x00\x00", 4);
			memcpy(mono+4, G1A(gxa)->header.icon, 68);
			memcpy(mono+72, "\x7f\xff\xff\xfc", 4);
			uint8_t *rgb24 = icon_conv_1to24(mono, 30, 19);

			/* Calculate a default output name if none is given */
			char *alloc = NULL;
			if(!output) {
				alloc = malloc(strlen(argv[optind]) + 5);
				if(!alloc) {
					fprintf(stderr, "error: %m\n");
					return 1;
				}
				default_output(argv[optind], ".png", alloc);
			}

			icon_save(output ? output : alloc, rgb24, 30, 19);
			free(alloc);
			free(rgb24);
		}
		else if(is_g3a(gxa)) {
			uint8_t *rgb24_uns = icon_conv_16to24(
				G3A(gxa)->header.icon_uns, 92, 64);
			uint8_t *rgb24_sel = icon_conv_16to24(
				G3A(gxa)->header.icon_sel, 92, 64);

			if(output_uns)
				icon_save(output_uns, rgb24_uns, 92, 64);
			if(output_sel)
				icon_save(output_sel, rgb24_sel, 92, 64);
			if(!output_uns && !output_sel) fprintf(stderr, "Please"
				" specify --output-uns or --output-sel.\n");

			free(rgb24_uns);
			free(rgb24_sel);
		}

	}

	return 0;
}
