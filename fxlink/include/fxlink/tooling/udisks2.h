//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//
// fxlink.tooling.udisks2: Utilities based on the UDisks2 library

#pragma once
#include <fxlink/config.h>
#ifndef FXLINK_DISABLE_UDISKS2

#include <udisks/udisks.h>
#include <fxlink/filter.h>

/* ud2_properties(): Determine properties of a UDisks2 USB drive */
struct fxlink_filter ud2_properties(UDisksDrive *drive);

/* Initialize a connection to the UDisks2 service via D-Bus. */
int ud2_start(UDisksClient **udc_ptr, UDisksManager **udm_ptr);

/* Close the connection to the UDisks2 service. */
void ud2_end(UDisksClient *udc, UDisksManager *udm);

/* ud2_unique_matching(): Device matching the provided filter, if unique
   Similar to usb_unique_matching(), please refer to "usb.h" for details.
   There are just many more inputs and outputs. */
int ud2_unique_matching(struct fxlink_filter const *filter, UDisksClient *udc,
	UDisksManager *udm, UDisksBlock **block, UDisksDrive **drive,
	UDisksFilesystem **fs);

/* ud2_unique_wait(): Wait for a device matching the provided filter to connect
   Returns an FXLINK_FILTER_* code. If a unique device is found, sets *udc,
   *udm, *block, *drive and *fs accordingly. */
int ud2_unique_wait(struct fxlink_filter const *filter, delay_t *delay,
	UDisksClient *udc, UDisksManager *udm, UDisksBlock **block,
	UDisksDrive **drive, UDisksFilesystem **fs);

//---
// Iteration on UDisks2 devices
//---

typedef struct {
	/* Current block, associated drive and filesystem */
	UDisksBlock *block;
	UDisksDrive *drive;
	UDisksFilesystem *fs;
	/* Device properties */
	struct fxlink_filter props;
	/* Whether the iteration has finished */
	bool done;

	/* Internal indicators: list of devices and current index */
	gchar **devices;
	int index;
	/* Client for object queries */
	UDisksClient *udc;

} ud2_iterator_t;

/* ud2_iter_start(): Start an iteration on UDisks2 devices
   If the first step fails, returns an iterator with (done = true) and sets
   (*error) to true; otherwise, sets (*error) to false. */
ud2_iterator_t ud2_iter_start(UDisksClient *udc, UDisksManager *udm,
	bool *error);

/* ud2_iter_next(): Iterate to the next UDisks2 device */
void ud2_iter_next(ud2_iterator_t *it);

/* Convenience for-loop macro for iteration */
#define for_udisks2_devices(NAME, udc, udm, error) \
	for(ud2_iterator_t NAME = ud2_iter_start(udc, udm, error); \
		!NAME.done; ud2_iter_next(&NAME)) if(!NAME.done)

#endif /* FXLINK_DISABLE_UDISKS2 */
