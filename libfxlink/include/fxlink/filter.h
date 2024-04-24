//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//
// fxlink.filter: Backend-agnostic device filters
//
// Most automatic functions in fxlink only connect with a single calculator.
// When several calculators are connected it is useful to narrow down the
// targeted machine programmatically, which is done with a device filter.
//
// A device filter is a simple conjunction of properties detected with the
// generic metadata of the device, as provided by the backend's API. Depending
// on the backend not all properties can be detected; for instance, fx-CG and
// G-III calculators have the same idVendor/idProduct pair and cannot be
// distinguished on the device descriptor alone, however they report different
// identities in SCSI queries and can be differentiated with UDisks2.
//
// The same type `struct fxlink_properties` is used both to represents devices'
// properties and filters. Every property has a "total information order" where
// values can be compared for how specific they are. A device matches a filter
// if its detected properties are more specific than the filter's requirements.
// So far the order is:
// - For booleans: true more specific than false
// - For serial number: any string more specific than NULL
//---

#pragma once
#include <fxlink/defs.h>
#include <stdio.h>

/* Detected devices' properties; also used for filters. */
struct fxlink_filter {
    /* The calculator uses Protocol 7/CESG502. Detected with idProduct 0x6101.
       These calcs don't support SCSI, so this is always false in UDisks2. */
    bool p7;
    /* The calculator supports Mass Storage/SCSI. Detected with idProduct
       0x6102. Always true in UDisks2. */
    bool mass_storage;
    /* The calculator has an fxlink interface. Always false in UDisks2. */
    bool intf_fxlink;
    /* The calculator has a CESG502 interface. Always false in UDisks2. */
    bool intf_cesg502;
    /* The calculator is from the fx-CG/G-III series. Detected with the SCSI
       drive `model` metadata. Only available in UDisks2. */
    bool series_cg;
    bool series_g3;
    /* Serial number. Requires write access to obtain in libusb (with a STRING
       descriptor request) because it's not cached. free() after use. */
    char *serial;
};

/* Return values for back-end specific matching functions. */
enum {
    /* A unique calculator matching the filter was found */
    FXLINK_FILTER_UNIQUE,
    /* No calculator matching the filter was found */
    FXLINK_FILTER_NONE,
    /* Multiple calculators matching the filter were found */
    FXLINK_FILTER_MULTIPLE,
    /* An error occurred while trying to enumerate devices */
    FXLINK_FILTER_ERROR,
};

/* Parse a filter string into a structure */
struct fxlink_filter *fxlink_filter_parse(char const *specification);

/* Disable filter properties not supported by each backend (with a warning
  emitted for each such ignored property) */
void fxlink_filter_clean_libusb(struct fxlink_filter *filter);
void fxlink_filter_clean_udisks2(struct fxlink_filter *filter);

/* Determines whether a set of concrete properties matches a filter. Returns
   whether all the fields of `props` are more specific than their counterparts
   in `filter`. */
bool fxlink_filter_match(
    struct fxlink_filter const *props,
    struct fxlink_filter const *filter);

/* Print a set of properties to a stream. Outputs a single line. */
void fxlink_filter_print(FILE *fp, struct fxlink_filter const *props);

/* Free a filter structure and its fields */
void fxlink_filter_free(struct fxlink_filter *filter);
