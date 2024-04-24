//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//

#include <fxlink/config.h>
#ifndef FXLINK_DISABLE_UDISKS2

#include <fxlink/tooling/udisks2.h>
#include <fxlink/filter.h>
#include <fxlink/logging.h>

int ud2_start(UDisksClient **udc_ptr, UDisksManager **udm_ptr)
{
	GError *error = NULL;

	UDisksClient *udc = udisks_client_new_sync(NULL, &error);
	if(error)
		return elog("cannot open udisks2 client: %s\n", error->message);

	UDisksManager *udm = udisks_client_get_manager(udc);
	if(!udm) {
		g_object_unref(udc);
		return elog("udisks2 daemon does not seem to be running\n");
	}

	*udc_ptr = udc;
	*udm_ptr = udm;
	return 0;
}

void ud2_end(UDisksClient *udc, UDisksManager *udm)
{
	(void)udm;
	g_object_unref(udc);
}

gchar **ud2_block_devices(UDisksManager *udm)
{
	gchar **blocks = NULL;
	GVariant *args = g_variant_new("a{sv}", NULL);
	GError *error = NULL;

	udisks_manager_call_get_block_devices_sync(udm,args,&blocks,NULL,&error);
	if(error)
		elog("cannot list udisks2 block devices: %s\n", error->message);

	return blocks;
}

UDisksBlock *ud2_block(UDisksClient *udc, gchar const *name)
{
	UDisksObject *obj = udisks_client_get_object(udc, name);
	return obj ? udisks_object_get_block(obj) : NULL;
}

UDisksDrive *ud2_drive(UDisksClient *udc, gchar const *name)
{
	UDisksObject *obj = udisks_client_get_object(udc, name);
	return obj ? udisks_object_get_drive(obj) : NULL;
}

UDisksFilesystem *ud2_filesystem(UDisksClient *udc, gchar const *name)
{
	UDisksObject *obj = udisks_client_get_object(udc, name);
	return obj ? udisks_object_get_filesystem(obj) : NULL;
}

//---
// Matching and properties
//---

bool is_casio_drive(UDisksDrive *drive)
{
	return strstr(udisks_drive_get_vendor(drive), "CASIO") != NULL;
}

struct fxlink_filter ud2_properties(UDisksDrive *drive)
{
	struct fxlink_filter props = { 0 };
	props.p7 = false;
	props.mass_storage = true;

	if(!strcmp(udisks_drive_get_model(drive), "ColorGraph"))
		props.series_cg = true;
	else if(!strcmp(udisks_drive_get_model(drive), "Calculator"))
		props.series_g3 = true;

	gchar const *s = udisks_drive_get_serial(drive);
	/* LINK sends a 12-byte serial number with four leading 0. Remove them */
	if(s && strlen(s) == 12 && !strncmp(s, "0000", 4)) s += 4;
	props.serial = strdup(s);

	return props;
}

int ud2_unique_matching(struct fxlink_filter const *filter, UDisksClient *udc,
	UDisksManager *udm, UDisksBlock **block_ptr, UDisksDrive **drive_ptr,
	UDisksFilesystem **fs_ptr)
{
	int status = FXLINK_FILTER_NONE;
	bool error;

	UDisksBlock *block = NULL;
	UDisksDrive *drive = NULL;
	UDisksFilesystem *fs = NULL;

	for_udisks2_devices(it, udc, udm, &error) {
		if(!fxlink_filter_match(&it.props, filter))
			continue;

		/* Already found a device before */
		if(status == FXLINK_FILTER_UNIQUE) {
			status = FXLINK_FILTER_MULTIPLE;
			g_object_unref(fs);
			g_object_unref(drive);
			g_object_unref(block);
			block = NULL;
			drive = NULL;
			fs = NULL;
			break;
		}

		/* First device: record it */
		block = g_object_ref(it.block);
		drive = g_object_ref(it.drive);
		fs = g_object_ref(it.fs);
		status = FXLINK_FILTER_UNIQUE;
	}
	if(error)
		return FXLINK_FILTER_ERROR;

	if(block_ptr)
		*block_ptr = block;
	else if(block)
		g_object_unref(block);

	if(drive_ptr)
		*drive_ptr = drive;
	else if(drive)
		g_object_unref(drive);

	if(fs_ptr)
		*fs_ptr = fs;
	else if(fs)
		g_object_unref(fs);

	return status;
}

int ud2_unique_wait(struct fxlink_filter const *filter, delay_t *delay,
	UDisksClient *udc, UDisksManager *udm, UDisksBlock **block,
	UDisksDrive **drive, UDisksFilesystem **fs)
{
	while(true) {
		int rc = ud2_unique_matching(filter, udc, udm, block, drive, fs);
		if(rc != FXLINK_FILTER_NONE) return rc;
		if(delay_cycle(delay)) return FXLINK_FILTER_NONE;
		udisks_client_settle(udc);
	}
}

//---
// Iteration on UDisks2 devices
//---

ud2_iterator_t ud2_iter_start(UDisksClient *udc, UDisksManager *udm,
	bool *error)
{
	ud2_iterator_t it = { .udc = udc };

	it.devices = ud2_block_devices(udm);
	if(!it.devices) {
		it.done = true;
		if(error) *error = true;
		return it;
	}

	it.index = -1;
	ud2_iter_next(&it);
	if(error) *error = false;
	return it;
}

void ud2_iter_next(ud2_iterator_t *it)
{
	if(it->done == true) return;

	/* Free the resources from the previous iteration */
	if(it->fs) g_object_unref(it->fs);
	if(it->drive) g_object_unref(it->drive);
	if(it->block) g_object_unref(it->block);
	it->block = NULL;
	it->drive = NULL;
	it->fs = NULL;

	/* Load the next device */
	if(!it->devices[++it->index]) {
		it->done = true;
	}
	else {
		gchar const *path = it->devices[it->index];

		it->block = ud2_block(it->udc, path);
		if(!it->block) return ud2_iter_next(it);

		/* Skip non-CASIO devices right away */
		it->drive = ud2_drive(it->udc, udisks_block_get_drive(it->block));
		if(!it->drive || !is_casio_drive(it->drive))
			return ud2_iter_next(it);

		/* Only consider file systems (not partition tables) */
		it->fs = ud2_filesystem(it->udc, path);
		if(!it->fs) return ud2_iter_next(it);

		it->props = ud2_properties(it->drive);
	}

	if(it->done)
		g_strfreev(it->devices);
}

//---
// Main functions
//---

int main_blocks(struct fxlink_filter *filter, delay_t *delay)
{
	fxlink_filter_clean_udisks2(filter);

	UDisksClient *udc = NULL;
	UDisksManager *udm = NULL;
	if(ud2_start(&udc, &udm)) return 1;

	ud2_unique_wait(filter, delay, udc, udm, NULL, NULL, NULL);

	int total_devices = 0;
	bool error;

	for_udisks2_devices(it, udc, udm, &error) {
		if(!fxlink_filter_match(&it.props, filter))
			continue;

		if(total_devices > 0) printf("\n");

		if(it.props.series_cg)
			printf("fx-CG series USB Mass Storage filesystem\n");
		else if(it.props.series_g3)
			printf("G-III series USB Mass Storage filesystem\n");
		else
			printf("Unknown USB Mass Storage filesystem\n");

		printf("        Block device:  %s\n",
			udisks_block_get_device(it.block));
		printf("      Identification:  Vendor: %s, Model: %s\n",
			udisks_drive_get_vendor(it.drive),
			udisks_drive_get_model(it.drive));

		if(it.props.serial)
			printf("       Serial number:  %s\n", it.props.serial);

		gchar const * const * mount_points =
			udisks_filesystem_get_mount_points(it.fs);
		if(!mount_points || !mount_points[0]) {
			printf("             Mounted:  no\n");
		}
		else for(int i = 0; mount_points[i]; i++) {
			printf("          Mounted at:  %s\n", mount_points[i]);
		}

		printf("          Properties:  ");
		fxlink_filter_print(stdout, &it.props);
		printf("\n");

		total_devices++;
	}

	if(!error && !total_devices)
		printf("No%s device found.\n", filter ? " matching" : "");

	ud2_end(udc, udm);
	return 0;
}

#endif /* FXLINK_DISABLE_UDISKS2 */
