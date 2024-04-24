//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//

#include <fxlink/filter.h>
#include <fxlink/logging.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//---
// Filter parser
//---

/* Identify property separating characters to be skipped */
static bool issep(int c)
{
	return (c == ' ' || c == '\t' || c == '\n' || c == ',');
}
/* Identify valid word characters for the filter */
static bool isword(int c)
{
	return c && !strchr(" \t\n,=", c);
}
/* Copy the next word in the string, assumes word is non-empty */
static char *read_word(char const **input)
{
	char const *str = *input;
	while(**input && isword(**input)) (*input)++;
	return strndup(str, *input - str);
}

/* Reads a property from the input source. Advances *input and sets *name and
   *value to newly-allocated copies of the name and (optional) value of the
   property (T_PROP). Both should be free()'d after use. At the end of the
   input, returns false and sets *name = *value = NULL. */
static bool read_property(char const **input, char **name, char **value)
{
	*name = *value = NULL;

	while(issep(**input))
		(*input)++;
	if(!**input)
		return false;

	if(!isword(**input)) {
		elog("expected property name in filter, found '%c'; stopping\n",
			**input);
		return false;
	}
	*name = read_word(input);

	if(**input == '=') {
		(*input)++;
		*value = read_word(input);
	}
	return true;
}

struct fxlink_filter *fxlink_filter_parse(char const *input)
{
	char *name=NULL, *value=NULL;
	struct fxlink_filter *filter = calloc(1, sizeof *filter);
	if(!filter)
		return NULL;

	while(read_property(&input, &name, &value)) {
		/* Add a new property to the current option */
		if(!strcmp(name, "p7") && !value)
			filter->p7 = true;
		else if(!strcmp(name, "mass_storage") && !value)
			filter->mass_storage = true;
		else if(!strcmp(name, "series_cg") && !value)
			filter->series_cg = true;
		else if(!strcmp(name, "series_g3") && !value)
			filter->series_g3 = true;
		else if(!strcmp(name, "intf_fxlink") && !value)
			filter->intf_fxlink = true;
		else if(!strcmp(name, "intf_cesg502") && !value)
			filter->intf_cesg502 = true;
		else if(!strcmp(name, "serial") && value)
			filter->serial = strdup(value);
		else if(!strcmp(name, "serial_number") && value) // Old name
			filter->serial = strdup(value);
		else wlog("ignoring invalid filter property: '%s' (%s value)\n",
			name, value ? "with" : "without");

		free(name);
		free(value);
	}
	return filter;
}

//---
// Filtering API
//---

void fxlink_filter_clean_libusb(struct fxlink_filter *filter)
{
	if(!filter)
		return;

	/* Suppress series_cg and series_g3, which are based off the SCSI metadata
	   provided only to UDisks2 */
	if(filter->series_cg) {
		wlog("ignoring series_cg in libusb filter (cannot be detected)\n");
		filter->series_cg = false;
	}
	if(filter->series_g3) {
		wlog("ignoring series_g3 in libusb filter (cannot be detected)\n");
		filter->series_g3 = false;
	}
}

void fxlink_filter_clean_udisks2(struct fxlink_filter *filter)
{
	/* Every property can be used */
	(void)filter;
}

bool fxlink_filter_match(
    struct fxlink_filter const *props,
    struct fxlink_filter const *filter)
{
	/* No filter is a pass-through */
	if(!filter)
		return true;

	if(filter->p7 && !props->p7)
		return false;
	if(filter->mass_storage && !props->mass_storage)
		return false;
	if(filter->intf_fxlink && !props->intf_fxlink)
		return false;
	if(filter->intf_cesg502 && !props->intf_cesg502)
		return false;
	if(filter->series_cg && !props->series_cg)
		return false;
	if(filter->series_g3 && !props->series_g3)
		return false;
	if(filter->serial &&
			(!props->serial || strcmp(filter->serial, props->serial)))
		return false;

	return true;
}

void fxlink_filter_print(FILE *fp, struct fxlink_filter const *filter)
{
	#define output(...) {           \
		if(sep) fprintf(fp, ", ");  \
		fprintf(fp, __VA_ARGS__);   \
		sep = true;                 \
	}

	bool sep = false;
	if(filter->p7)
		output("p7");
	if(filter->mass_storage)
		output("mass_storage");
	if(filter->intf_fxlink)
		output("intf_fxlink");
	if(filter->intf_cesg502)
		output("intf_cesg502");
	if(filter->series_cg)
		output("series_cg");
	if(filter->series_g3)
		output("series_g3");
	if(filter->serial)
		output("serial=%s", filter->serial);
}

void fxlink_filter_free(struct fxlink_filter *filter)
{
	if(filter)
		free(filter->serial);
	free(filter);
}
