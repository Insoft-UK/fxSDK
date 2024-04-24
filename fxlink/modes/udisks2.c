//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//

#include <fxlink/config.h>
#ifndef FXLINK_DISABLE_UDISKS2

#include "../fxlink.h"
#include <fxlink/tooling/udisks2.h>
#include <fxlink/filter.h>
#include <fxlink/logging.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main_send(struct fxlink_filter *filter, delay_t *delay, char **files)
{
	fxlink_filter_clean_udisks2(filter);
	GError *error = NULL;
	char **argv = NULL;
	int rc = 0;

	UDisksClient *udc = NULL;
	UDisksManager *udm = NULL;
	if(ud2_start(&udc, &udm)) return 1;

	UDisksBlock *block = NULL;
	UDisksDrive *drive = NULL;
	UDisksFilesystem *fs = NULL;
	rc = ud2_unique_wait(filter, delay, udc, udm, &block, &drive, &fs);
	if(rc != FXLINK_FILTER_UNIQUE) {
		rc = 1;
		goto end;
	}

	/* Determine a mount folder, mounting the volume if needed */
	gchar *folder = NULL;

	gchar const *dev = udisks_block_get_device(block);
	gchar const * const * mount_points =
		udisks_filesystem_get_mount_points(fs);

	if(!mount_points || !mount_points[0]) {
		GVariant *args = g_variant_new("a{sv}", NULL);
		udisks_filesystem_call_mount_sync(fs, args, &folder, NULL, &error);
		if(error) {
			rc = elog("cannot mount %s: %s\n", dev, error->message);
			goto end;
		}
		printf("Mounted %s to %s.\n", dev, folder);
	}
	else {
		folder = strdup(mount_points[0]);
		printf("Already mounted at %s.\n", folder);
	}

	/* Copy files with external cp(1) */
	int file_count = 0;
	while(files[file_count]) file_count++;

	argv = malloc((file_count + 3) * sizeof *argv);
	if(!argv) {
		rc = elog("cannot allocate argv array for cp(1)\n");
		goto end;
	}
	argv[0] = "cp";
	for(int i = 0; files[i]; i++)
		argv[i+1] = files[i];
	argv[file_count+1] = folder;
	argv[file_count+2] = NULL;

	/* Print command */
	printf("Running cp");
	for(int i = 1; argv[i]; i++) printf(" '%s'", argv[i]);
	printf("\n");

	pid_t pid = fork();

	if(pid == 0) {
		execvp("cp", argv);
	}
	else if(pid == -1) {
		rc = elog("failed to fork to invoke cp\n");
		goto end;
	}
	else {
		int wstatus;
		waitpid(pid, &wstatus, 0);

		if(!WIFEXITED(wstatus))
			elog("process did not terminate normally\n");
		else if(WEXITSTATUS(wstatus) != 0) {
			elog("process terminated with error %d\n", WEXITSTATUS(wstatus));
		}
	}

	/* Unmount the filesystem and eject the device */
	GVariant *args = g_variant_new("a{sv}", NULL);
	udisks_filesystem_call_unmount_sync(fs, args, NULL, &error);
	if(error)
		elog("while unmounting %s: %s\n", dev, error->message);
	else
		printf("Unmounted %s.\n", dev);

	args = g_variant_new("a{sv}", NULL);
	udisks_drive_call_power_off_sync(drive, args, NULL, &error);
	if(error)
		elog("while ejecting %s: %s\n", dev, error->message);
	else
		printf("Ejected %s.\n", dev);

end:
	free(folder);
	if(argv) free(argv);
	if(fs) g_object_unref(fs);
	if(drive) g_object_unref(drive);
	if(block) g_object_unref(block);
	if(udc && udm) ud2_end(udc, udm);
	return rc;
}

#endif /* FXLINK_DISABLE_UDISKS2 */
