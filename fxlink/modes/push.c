//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//

#include "../fxlink.h"
#include <fxlink/filter.h>
#include <fxlink/logging.h>
#include <fxlink/devices.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int main_push(struct fxlink_filter *filter, delay_t *delay,
	libusb_context *ctx, char **files)
{
	int rc = 1;
	struct fxlink_device *fdev = NULL;

	/* Load binary file */
	FILE *fp = fopen(files[0], "rb");
	if (!fp) {
		printf("error: Unable to open file %s\n", files[0]);
		goto end;
	}

	fseek(fp, 0, SEEK_END);
	long fsize = ftell(fp);
	// If more than 6MB, abort
	if (fsize > 6 * 1024 * 1024) {
		printf("error: File is too large (max 6MB)\n");
		goto end;
	}
	fseek(fp, 0, SEEK_SET);
	uint8_t *filebuf = malloc(fsize);
	fread(filebuf, fsize, 1, fp);
	fclose(fp);

	/* Wait for a device to be connected */
	fxlink_filter_clean_libusb(filter);
	filter->intf_cesg502 = true;

	fdev = fxlink_device_find_wait(ctx, filter, delay);
	if(!fdev) {
		printf("No device found.\n");
		return 1;
	}

	/* The device uses CESG502, so drive the interface manually */
	if((rc = libusb_claim_interface(fdev->dh, 0))) {
		hlog("calculators %s", fxlink_device_id(fdev));
		rc = elog_libusb(rc, "cannot claim interface 0");
		goto end;
	}

	hlog("push");
	log_("connected to %s\n", fxlink_device_id(fdev));

	// Wait to receive "USB loader ready" over USB bulk transfer
	uint8_t buf[18];
	while (1)
	{
		int actual_length;
		rc = libusb_bulk_transfer(fdev->dh, 0x82, buf, sizeof(buf) - 1,
			&actual_length, 0);
		buf[sizeof(buf) - 1] = 0;
		// if (rc == LIBUSB_ERROR_TIMEOUT) continue;
		if (rc) {
			rc = elog_libusb(rc, "cannot receive data");
			goto end;
		}
		if (actual_length == 0) continue;
		if (actual_length != 17) {
			printf("error: Received %d bytes, expected 17\n", actual_length);
			goto end;
		}
		// See if it's the "USB loader ready" message with strcmp
		if (strcmp((char*) buf, "USB loader ready") == 0) {
			printf("Ready to send!\n");
			break;
		} else {
			printf("error: Unknown message received: %s\n", buf);
			goto end;
		}
	}

	// Send the contents of the passed file over USB bulk transfer

	// First send the size of the file
	uint8_t sizebuf[4];
	sizebuf[0] = (fsize >> 24) & 0xFF;
	sizebuf[1] = (fsize >> 16) & 0xFF;
	sizebuf[2] = (fsize >> 8) & 0xFF;
	sizebuf[3] = fsize & 0xFF;
	rc = libusb_bulk_transfer(fdev->dh, 0x01, sizebuf, sizeof(sizebuf), NULL,
		0);
	if (rc) {
		rc = elog_libusb(rc, "cannot send size");
		goto end;
	}

	// Then send the file contents
	printf("Sending %ld bytes\n", fsize);
	int sent = 0;
	while (sent < fsize) {
		int actual_length;
		rc = libusb_bulk_transfer(fdev->dh, 0x01, filebuf + sent, fsize - sent,
			&actual_length, 0);
		if (rc) {
			rc = elog_libusb(rc, "cannot send data");
			goto end;
		}
		sent += actual_length;
	}
	printf("Sent %d bytes\n", sent);

end:
	if(fdev) {
		libusb_release_interface(fdev->dh, 0);
		fxlink_device_cleanup(fdev);
		free(fdev);
	}
	return rc;
}
