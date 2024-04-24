//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//

#include <fxlink/devices.h>
#include <fxlink/logging.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

char const *fxlink_device_id(struct fxlink_device const *fdev)
{
    static char str[32];
    sprintf(str, "%03d:%03d", fdev->busNumber, fdev->deviceAddress);
    return str;
}

char const *fxlink_device_status_string(struct fxlink_device const *fdev)
{
    switch(fdev->status) {
    case FXLINK_FDEV_STATUS_PENDING:
        return "PENDING";
    case FXLINK_FDEV_STATUS_IGNORED:
        return "IGNORED";
    case FXLINK_FDEV_STATUS_ERROR:
        return "ERROR";
    case FXLINK_FDEV_STATUS_IDLE:
        return "IDLE";
    case FXLINK_FDEV_STATUS_CONNECTED:
        return "CONNECTED";
    default:
        return "<INVALID>";
    }
}

char const *fxlink_device_system_string(struct fxlink_device const *fdev)
{
    if(!fdev->calc)
        return "<!CALC>";

    switch(fdev->calc->system) {
    case FXLINK_CALC_SYSTEM_UNKNOWN:
        return "UNKNOWN";
    case FXLINK_CALC_SYSTEM_LINKSCSI:
        return "LINKSCSI";
    case FXLINK_CALC_SYSTEM_CESG502:
        return "CESG502";
    case FXLINK_CALC_SYSTEM_GINT:
        return "GINT";
    default:
        return "<INVALID>";
    }
}

static bool is_casio_calculator(int idVendor, int idProduct)
{
    return idVendor == 0x07cf && (idProduct == 0x6101 || idProduct == 0x6102);
}

static struct libusb_config_descriptor *active_config_descriptor(
    struct fxlink_device const *fdev)
{
    struct libusb_config_descriptor *cd = NULL;
    int rc = libusb_get_active_config_descriptor(fdev->dp, &cd);
    if(rc != 0) {
        if(rc != LIBUSB_ERROR_NOT_FOUND)
            elog_libusb(rc, "cannot request config descriptor");
        return NULL;
    }
    return cd;
}

/* Gather a list of a classes; either one is specified for the entire device in
   the device descriptor, either one is specified for each interface in
   interface descriptors. */
static bool find_interface_classes(struct fxlink_device *fdev)
{
    struct fxlink_calc *calc = fdev->calc;
    struct libusb_device_descriptor dc;
    libusb_get_device_descriptor(fdev->dp, &dc);

    /* Fixed class specified in the device descriptor */
    if(dc.bDeviceClass != LIBUSB_CLASS_PER_INTERFACE) {
        calc->interface_count = 1;
        calc->classes = malloc(1 * sizeof *calc->classes);
        calc->classes[0] = (dc.bDeviceClass << 8) | dc.bDeviceSubClass;
        return true;
    }

    /* Class specified by the interface descriptors */
    struct libusb_config_descriptor *cd = active_config_descriptor(fdev);
    if(!cd)
        return false;

    calc->interface_count = cd->bNumInterfaces;
    calc->classes = malloc(cd->bNumInterfaces * sizeof *calc->classes);

    for(int i = 0; i < cd->bNumInterfaces; i++) {
        struct libusb_interface const *intf = &cd->interface[i];
        struct libusb_interface_descriptor const *id = &intf->altsetting[0];

        calc->classes[i] = (id->bInterfaceClass << 8) | id->bInterfaceSubClass;
        if(calc->classes[i] == 0xff77 && calc->fxlink_inum < 0)
            calc->fxlink_inum = i;
    }
    return true;
}

/* Find the endpoints for the fxlink interface. */
static bool find_fxlink_endpoints(struct fxlink_device *fdev, bool quiet)
{
    struct fxlink_calc *calc = fdev->calc;
    struct fxlink_comm *comm = calloc(1, sizeof *comm);
    if(!comm) {
        elog("find_fxlink_endpoints(): %m\n");
        return false;
    }
    fdev->comm = comm;
    comm->ep_bulk_IN = 0xff;
    comm->ep_bulk_OUT = 0xff;

    struct libusb_config_descriptor *cd = active_config_descriptor(fdev);
    if(!cd) {
        free(comm);
        return false;
    }

    struct libusb_interface const *intf = &cd->interface[calc->fxlink_inum];
    struct libusb_interface_descriptor const *id = &intf->altsetting[0];

    if(!quiet) {
        hlog("calculators %s", fxlink_device_id(fdev));
        log_("fxlink interface has %d endpoints\n", id->bNumEndpoints);
    }

    for(int i = 0; i < id->bNumEndpoints; i++) {
        struct libusb_endpoint_descriptor const *ed = &id->endpoint[i];

        int dir = ed->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK;
        int type = ed->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK;

        if(dir == LIBUSB_ENDPOINT_OUT
                && type == LIBUSB_TRANSFER_TYPE_BULK)
            comm->ep_bulk_OUT = ed->bEndpointAddress;
        if(dir == LIBUSB_ENDPOINT_IN
                && type == LIBUSB_TRANSFER_TYPE_BULK)
            comm->ep_bulk_IN = ed->bEndpointAddress;
    }

    return true;
}

/* Determine system based on interface heuristics */
static void determine_system_type(struct fxlink_device *fdev)
{
    struct fxlink_calc *calc = fdev->calc;
    if(!calc)
        return;

    calc->system = FXLINK_CALC_SYSTEM_UNKNOWN;

    /* Single class of type SCSI -> LINKSCSI */
    if(calc->interface_count == 1 && calc->classes[0] == 0x0806) {
        calc->system = FXLINK_CALC_SYSTEM_LINKSCSI;
        return;
    }
    /* Single class of type vendor-specific 00 -> CESG502 */
    if(calc->interface_count == 1 && calc->classes[0] == 0xff00) {
        calc->system = FXLINK_CALC_SYSTEM_CESG502;
        return;
    }
    /* Has an fxlink interface -> GINT */
    for(int i = 0; i < calc->interface_count; i++) {
        if(calc->classes[i] == 0xff77) {
            calc->system = FXLINK_CALC_SYSTEM_GINT;
            return;
        }
    }
}

/* Retrieve the serial. Returns a duplicated copy, NULL on error. */
static char *retrieve_serial_number(struct fxlink_device *fdev)
{
    if(!fdev->dh)
        return NULL;

    struct libusb_device_descriptor dc;
    libusb_get_device_descriptor(fdev->dp, &dc);

    if(!dc.iSerialNumber)
        return NULL;

    char str[256];
    int rc = libusb_get_string_descriptor_ascii(fdev->dh, dc.iSerialNumber,
        (void *)str, 256);
    if(rc < 0) {
        wlog_libusb(rc, "could not retrieve serial number");
        return NULL;
    }

    /* LINK sends a 12-byte serial number with four leading 0. Remove them */
    char *serial = str;
    if(rc == 12 && !strncmp(serial, "0000", 4))
        serial += 4;
    return strdup(serial);
}

void fxlink_device_analysis_1(struct fxlink_device *fdev, bool quiet)
{
    struct fxlink_calc *calc = calloc(1, sizeof *calc);
    if(!calc) {
        elog("analyze_calculator(): %m\n");
        return;
    }

    fdev->calc = calc;
    calc->system = FXLINK_CALC_SYSTEM_UNKNOWN;
    calc->fxlink_inum = -1;
    if(!find_interface_classes(fdev)) {
        fdev->status = FXLINK_FDEV_STATUS_ERROR;
        return;
    }

    if(!quiet) {
        hlog("calculators %s", fxlink_device_id(fdev));
        log_("%1$d interface%2$s, class code%2$s", calc->interface_count,
            calc->interface_count != 1 ? "s" : "");

        for(int i = 0; i < calc->interface_count; i++) {
            log_(" %02x.%02x", calc->classes[i] >> 8, calc->classes[i] & 0xff);
            if(i == calc->fxlink_inum)
                log_("(*)");
        }
        log_("\n");
    }

    determine_system_type(fdev);

    /* Don't open SCSI devices because we can't interact with them (the kernel
       already claims the interface) and doing so will prevent them from
       subspending and disconnecting */
    if(calc->system == FXLINK_CALC_SYSTEM_LINKSCSI) {
        fdev->status = FXLINK_FDEV_STATUS_IGNORED;
        return;
    }

    if(calc->fxlink_inum >= 0 && !find_fxlink_endpoints(fdev, quiet)) {
        hlog("calculators %s", fxlink_device_id(fdev));
        elog("non-conforming fxlink interface!\n");
        fdev->status = FXLINK_FDEV_STATUS_ERROR;
        return;
    }

    int rc = libusb_open(fdev->dp, &fdev->dh);

    if(!quiet)
        hlog("calculators %s", fxlink_device_id(fdev));
    if(rc != 0) {
        elog("opening device failed: %s\n", libusb_strerror(rc));
        fdev->status = FXLINK_FDEV_STATUS_ERROR;
        return;
    }
    if(!quiet)
        log_("successfully opened device!\n");

    /* Don't detach kernel drivers to avoid breaking the Mass Storage
       communications if fxlink is ever started while the native LINK
       application is running! */
    libusb_set_auto_detach_kernel_driver(fdev->dh, false);
}

void fxlink_device_analysis_2(struct fxlink_device *fdev)
{
    if(fdev->status != FXLINK_FDEV_STATUS_PENDING)
        return;

    fdev->calc->serial = retrieve_serial_number(fdev);
    fdev->status = FXLINK_FDEV_STATUS_IDLE;
}

bool fxlink_device_ready_to_connect(struct fxlink_device const *fdev)
{
    bool status = (fdev->status == FXLINK_FDEV_STATUS_IDLE) ||
                  (fdev->status == FXLINK_FDEV_STATUS_CONNECTED);
    return fdev->calc && fdev->comm && status;
}

bool fxlink_device_has_fxlink_interface(struct fxlink_device const *fdev)
{
    return fdev->calc && fdev->comm && (fdev->comm->ep_bulk_IN != 0xff);
}

bool fxlink_device_claim_fxlink(struct fxlink_device *fdev)
{
    if(!fxlink_device_ready_to_connect(fdev) ||
       !fxlink_device_has_fxlink_interface(fdev) ||
       fdev->comm->claimed)
        return false;

    /* Allocate transfer data */
    fdev->comm->buffer_IN_size = 2048;
    fdev->comm->buffer_IN = malloc(fdev->comm->buffer_IN_size);
    if(!fdev->comm->buffer_IN) {
        elog("could not allocate buffer for bulk IN transfer\n");
        fdev->status = FXLINK_FDEV_STATUS_ERROR;
        return false;
    }

    hlog("calculators %s", fxlink_device_id(fdev));
    int rc = libusb_claim_interface(fdev->dh, fdev->calc->fxlink_inum);
    if(rc != 0) {
        elog_libusb(rc, "claiming fxlink interface #%d failed",
            fdev->calc->fxlink_inum);
        fdev->status = FXLINK_FDEV_STATUS_ERROR;
        return false;
    }

    fdev->comm->claimed = true;
    fdev->status = FXLINK_FDEV_STATUS_CONNECTED;
    log_("successfully claimed interface!\n");
    return true;
}

void fxlink_device_get_properties(struct fxlink_device const *fdev,
    struct fxlink_filter *p)
{
    memset(p, 0, sizeof *p);

    if(fdev->idProduct == 0x6101)
        p->p7 = true;
    if(fdev->idProduct == 0x6102)
        p->mass_storage = true;
    if(fdev->calc) {
        p->serial = fdev->calc->serial;
        p->intf_cesg502 = (fdev->calc->system == FXLINK_CALC_SYSTEM_CESG502);
        p->intf_fxlink = (fdev->calc->fxlink_inum >= 0);
    }
}

void fxlink_device_interrupt_transfers(struct fxlink_device *fdev)
{
    struct fxlink_comm *comm = fdev->comm;
    if(!comm)
        return;

    /* Interrupt transfers if any are still running */
    if(comm->tr_bulk_IN && !comm->cancelled_IN) {
        libusb_cancel_transfer(comm->tr_bulk_IN);
        comm->cancelled_IN = true;
    }
    if(comm->tr_bulk_OUT && !comm->cancelled_OUT) {
        libusb_cancel_transfer(comm->tr_bulk_OUT);
        comm->cancelled_OUT = true;
    }
}

void fxlink_device_cleanup(struct fxlink_device *fdev)
{
    /* Close the device if it's open */
    if(fdev->dh) {
        /* Release the fxlink interface if it's claimed */
        if(fdev->comm && fdev->comm->claimed)
            libusb_release_interface(fdev->dh, fdev->calc->fxlink_inum);

        libusb_close(fdev->dh);
        fdev->dh = NULL;
    }

    /* Free associated memory */
    if(fdev->calc) {
        free(fdev->calc->classes);
        free(fdev->calc->serial);
        free(fdev->calc);
    }
    free(fdev->comm);

    /* Unreference libusb devices so it can also be freed */
    libusb_unref_device(fdev->dp);
    fdev->dp = NULL;
}

//---
// Bulk transfers
//---

/* Note: this function is run by the even handler and can't do any crazy libusb
   stuff like sync I/O or getting descriptors. */
static void bulk_IN_callback(struct libusb_transfer *transfer)
{
    struct fxlink_device *fdev = transfer->user_data;
    struct fxlink_comm *comm = fdev->comm;

    bool resubmit = false;
    void *data = transfer->buffer;
    int data_size = transfer->actual_length;

    if(transfer->status != LIBUSB_TRANSFER_COMPLETED)
        hlog("calculators %s", fxlink_device_id(fdev));

    switch(transfer->status) {
    case LIBUSB_TRANSFER_COMPLETED:
        /* Start or continue an fxlink transfer. When finished, don't resubmit,
           instead have the user pick up the message before continuing. */
        resubmit = true;
        if(comm->ftransfer_IN) {
            fxlink_transfer_receive(comm->ftransfer_IN, data, data_size);
            if(fxlink_transfer_complete(comm->ftransfer_IN))
                resubmit = false;
        }
        else {
            comm->ftransfer_IN = fxlink_transfer_make_IN(data, data_size);
        }
        break;

    /* Error: drop data and don't resubmit */
    case LIBUSB_TRANSFER_ERROR:
        log_("transfer error (%d bytes dropped)\n", data_size);
        break;
    /* Timeout: drop data, but resubmit */
    case LIBUSB_TRANSFER_TIMED_OUT:
        log_("transfer timed out (%d bytes dropped)\n", data_size);
        break;
    /* Cancelled transfer: drop data and don't try to resubmit */
    case LIBUSB_TRANSFER_CANCELLED:
        log_("transfer cancelled (%d bytes dropped)\n", data_size);
        break;
    /* Stall: treat as an error */
    case LIBUSB_TRANSFER_STALL:
        log_("transfer stalled (%d bytes dropped)\n", data_size);
        break;
    /* Overflow: treat as an error (should not happen because we set our buffer
       size to a multiple of the maximum packet size) */
    case LIBUSB_TRANSFER_OVERFLOW:
        log_("transfer overflowed (%d bytes dropped)\n", data_size);
        break;
    /* No device: this is normal */
    case LIBUSB_TRANSFER_NO_DEVICE:
        log_("stop listening (calculator disconnected)\n");
        break;
    }

    /* Resubmit transfer so we can get new data as soon as possible */
    if(resubmit) {
        libusb_submit_transfer(comm->tr_bulk_IN);
    }
    else {
        libusb_free_transfer(comm->tr_bulk_IN);
        comm->tr_bulk_IN = NULL;
    }
}

bool fxlink_device_start_bulk_IN(struct fxlink_device *fdev)
{
    if(!fdev->comm || !fdev->comm->claimed || fdev->comm->tr_bulk_IN)
        return false;

    fdev->comm->tr_bulk_IN = libusb_alloc_transfer(0);
    if(!fdev->comm->tr_bulk_IN) {
        elog("allocation of bulk IN transfer failed\n");
        return false;
    }

    libusb_fill_bulk_transfer(fdev->comm->tr_bulk_IN,
        fdev->dh,                       /* Device handle */
        fdev->comm->ep_bulk_IN,         /* Endpoint */
        fdev->comm->buffer_IN,          /* Buffer */
        fdev->comm->buffer_IN_size,     /* Buffer size */
        bulk_IN_callback, fdev,         /* Callback function and argument */
        -1                              /* Timeout */
    );

    int rc = libusb_submit_transfer(fdev->comm->tr_bulk_IN);
    if(rc < 0) {
        elog_libusb(rc, "bulk IN transfer failed to submit");
        fdev->status = FXLINK_FDEV_STATUS_ERROR;
        return false;
    }

    // hlog("calculators %s", fxlink_device_id(fdev));
    // log_("submitted new IN transfer (no timeout)\n");
    return true;
}

struct fxlink_message *fxlink_device_finish_bulk_IN(struct fxlink_device *fdev)
{
    struct fxlink_comm *comm = fdev->comm;

    if(!comm || !comm->ftransfer_IN)
        return NULL;
    if(!fxlink_transfer_complete(comm->ftransfer_IN))
        return NULL;

    struct fxlink_message *msg = fxlink_transfer_finish_IN(comm->ftransfer_IN);
    if(!msg)
        return NULL;

    int version_major = (msg->version >> 8) & 0xff;
    int version_minor = msg->version & 0xff;

    hlog("calculators %s", fxlink_device_id(fdev));
    log_("new message (v%d.%d): %.16s:%.16s, %s\n",
        version_major, version_minor,
        msg->application, msg->type, fxlink_size_string(msg->size));

    comm->ftransfer_IN = NULL;
    return msg;
}

/* Note: this function is run by the even handler and can't do any crazy libusb
   stuff like sync I/O or getting descriptors. */
static void bulk_OUT_callback(struct libusb_transfer *transfer)
{
    struct fxlink_device *fdev = transfer->user_data;
    struct fxlink_comm *comm = fdev->comm;

    int data_size = transfer->actual_length;
    bool send_more = true;
    struct fxlink_transfer *tr = comm->ftransfer_OUT;

    if(transfer->status != LIBUSB_TRANSFER_COMPLETED)
        hlog("calculators %s", fxlink_device_id(fdev));

    switch(transfer->status) {
    case LIBUSB_TRANSFER_COMPLETED:
        if(tr->processed_size < 0) {
            if(data_size != FXLINK_MESSAGE_HEADER_SIZE) {
                elog("OUT for header only partially completed");
                send_more = false;
            }
            tr->processed_size = 0;
        }
        else {
            tr->processed_size += data_size;
            if(fxlink_transfer_complete(tr))
                send_more = false;
        }

        if(send_more) {
            libusb_fill_bulk_transfer(comm->tr_bulk_OUT, fdev->dh,
                comm->ep_bulk_OUT,                  /* Endpoint */
                tr->msg.data + tr->processed_size,  /* Buffer */
                tr->msg.size - tr->processed_size,  /* Buffer size */
                bulk_OUT_callback, fdev, -1);       /* Callback and timeout */
            libusb_submit_transfer(comm->tr_bulk_OUT);
        }
        else {
            libusb_free_transfer(comm->tr_bulk_OUT);
            comm->tr_bulk_OUT = NULL;
            fxlink_transfer_free(comm->ftransfer_OUT);
            comm->ftransfer_OUT = NULL;
        }
        break;

    /* Typical errors */
    case LIBUSB_TRANSFER_ERROR:     log_("transfer error\n"); break;
    case LIBUSB_TRANSFER_TIMED_OUT: log_("transfer timed out\n"); break;
    case LIBUSB_TRANSFER_CANCELLED: log_("transfer cancelled\n"); break;
    case LIBUSB_TRANSFER_STALL:     log_("transfer stalled\n"); break;
    case LIBUSB_TRANSFER_OVERFLOW:  log_("transfer overflowed\n"); break;
    /* No device: this is normal */
    case LIBUSB_TRANSFER_NO_DEVICE:
        log_("stop listening (calculator disconnected)\n");
        break;
    }
}

bool fxlink_device_start_bulk_OUT(struct fxlink_device *fdev,
    char const *app, char const *type, void const *data, int size,
    bool own_data)
{
    struct fxlink_comm *comm = fdev->comm;
    if(!comm || !comm->claimed || comm->ftransfer_OUT)
        return false;

    comm->ftransfer_OUT =
        fxlink_transfer_make_OUT(app, type, data, size, own_data);
    if(!comm->ftransfer_OUT) {
        elog("allocation of OUT transfer (protocol) failed\n");
        return false;
    }

    comm->tr_bulk_OUT = libusb_alloc_transfer(0);
    if(!comm->tr_bulk_OUT) {
        elog("allocation of bulk OUT transfer (libusb) failed\n");
        free(comm->ftransfer_OUT);
        return false;
    }

    libusb_fill_bulk_transfer(comm->tr_bulk_OUT, fdev->dh,
        comm->ep_bulk_OUT,                  /* Endpoint */
        (void *)&comm->ftransfer_OUT->msg,  /* Buffer */
        FXLINK_MESSAGE_HEADER_SIZE,         /* Buffer size */
        bulk_OUT_callback, fdev, -1);       /* Callback and timeout */

    /* The fxlink protocol generally doesn't rely on sizes and instead expects
       zero-length packets to mark the ends of transactions */
    comm->tr_bulk_OUT->flags = LIBUSB_TRANSFER_ADD_ZERO_PACKET;

    int rc = libusb_submit_transfer(comm->tr_bulk_OUT);
    if(rc < 0) {
        elog_libusb(rc, "bulk OUT transfer failed to submit");
        fdev->status = FXLINK_FDEV_STATUS_ERROR;
        return false;
    }

    return true;
}

//---
// Polled file descriptor tracking
//---

static void generate_poll_fds(struct fxlink_pollfds *tracker)
{
    /* Get the set of libusb file descriptors to poll for news */
    struct libusb_pollfd const **usb_fds = libusb_get_pollfds(tracker->ctx);
    int usb_n = 0;

    if(!usb_fds) {
        elog("libusb_get_pollfds() returned NULL, devices will probably not "
            "be detected!\n");
        free(tracker->fds);
        tracker->fds = NULL;
        tracker->count = 0;
        return;
    }

    hlog("libusb");
    log_("fds to poll:");
    for(int i = 0; usb_fds[i] != NULL; i++) {
        log_(" %d(%s%s)",
            usb_fds[i]->fd,
            (usb_fds[i]->events & POLLIN) ? "i" : "",
            (usb_fds[i]->events & POLLOUT) ? "o" : "");
    }
    if(!usb_fds[0])
        log_(" (none)");
    log_("\n");

    while(usb_fds[usb_n])
        usb_n++;

    /* Allocate a bunch of `struct pollfd` and also monitor STDIN_FILENO */
    tracker->count = usb_n;
    tracker->fds = realloc(tracker->fds, usb_n * sizeof *tracker->fds);

    for(int i = 0; i < usb_n; i++) {
        tracker->fds[i].fd = usb_fds[i]->fd;
        tracker->fds[i].events = usb_fds[i]->events;
    }
}

static void handle_add_poll_fd(int fd, short events, void *data)
{
    (void)fd;
    (void)events;
    generate_poll_fds(data);
}

static void handle_remove_poll_fd(int fd, void *data)
{
    (void)fd;
    generate_poll_fds(data);
}

void fxlink_pollfds_track(struct fxlink_pollfds *tracker, libusb_context *ctx)
{
    memset(tracker, 0, sizeof *tracker);
    tracker->ctx = ctx;

    libusb_set_pollfd_notifiers(ctx, handle_add_poll_fd, handle_remove_poll_fd,
        tracker);
    generate_poll_fds(tracker);
}

void fxlink_pollfds_stop(struct fxlink_pollfds *tracker)
{
    libusb_set_pollfd_notifiers(tracker->ctx, NULL, NULL, NULL);
    free(tracker->fds);
    memset(tracker, 0, sizeof *tracker);
}

//---
// Device tracking
//---

static void enumerate_devices(libusb_context *ctx,
    struct fxlink_device_list *list)
{
    libusb_device **libusb_list = NULL;
    int new_count = libusb_get_device_list(ctx, &libusb_list);

    if(new_count < 0) {
        elog("libusb_get_device_list() failed with error %d\n", new_count);
        return;
    }

    /* We now diff the previous array with the current one */
    struct fxlink_device *new_fdevs = calloc(new_count, sizeof *new_fdevs);
    if(!new_fdevs) {
        elog("enumerate_devices(): %m\n");
        return;
    }
    int k = 0;

    /* First copy over any device that is still connected */
    for(int i = 0; i < list->count; i++) {
        struct fxlink_device *fdev = &list->devices[i];
        assert(fdev->dp != NULL);

        bool still_connected = false;
        for(int j = 0; j < new_count; j++)
            still_connected = still_connected || (libusb_list[j] == fdev->dp);

        if(still_connected)
            new_fdevs[k++] = list->devices[i];
        else {
            fxlink_device_cleanup(fdev);
            hlog("devices %s", fxlink_device_id(fdev));
            log_("disconnected\n");
        }
    }

    /* Then add all the new ones */
    for(int j = 0; j < new_count; j++) {
        libusb_device *dp = libusb_list[j];
        struct libusb_device_descriptor dc;

        bool already_known = false;
        for(int i = 0; i < list->count; i++)
            already_known = already_known || (list->devices[i].dp == dp);
        if(already_known)
            continue;

        libusb_ref_device(dp);
        libusb_get_device_descriptor(dp, &dc);

        new_fdevs[k].dp = dp;
        new_fdevs[k].status = FXLINK_FDEV_STATUS_PENDING;
        new_fdevs[k].busNumber = libusb_get_bus_number(dp);
        new_fdevs[k].deviceAddress = libusb_get_device_address(dp);
        new_fdevs[k].idVendor = dc.idVendor;
        new_fdevs[k].idProduct = dc.idProduct;
        new_fdevs[k].calc = NULL;
        new_fdevs[k].comm = NULL;

        hlog("devices %s", fxlink_device_id(&new_fdevs[k]));

        if(is_casio_calculator(dc.idVendor, dc.idProduct)) {
            log_("new CASIO calculator (%04x:%04x)\n",
                dc.idVendor, dc.idProduct);
            fxlink_device_analysis_1(&new_fdevs[k], false);
        }
        else {
            log_("new non-CASIO-calculator device (%04x:%04x)\n",
                dc.idVendor, dc.idProduct);
            new_fdevs[k].status = FXLINK_FDEV_STATUS_IGNORED;
        }
        k++;
    }

    assert(k == new_count);
    free(list->devices);
    list->devices = new_fdevs;
    list->count = new_count;
    libusb_free_device_list(libusb_list, true);
}

static int handle_hotplug(libusb_context *ctx, libusb_device *device,
    libusb_hotplug_event event, void *user_data)
{
    /* Note that due to threading considerations in libusb, a device may be
       notified for hotplugging twice, or may depart without ever having been
       notified for arrival. */
    (void)device;

    if(event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED)
        enumerate_devices(ctx, user_data);
    else if(event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT)
        enumerate_devices(ctx, user_data);
    else
        wlog("unhandled libusb hotplug event of type %d\n", event);

    return 0;
}

bool fxlink_device_list_track(struct fxlink_device_list *list,
    libusb_context *ctx)
{
    memset(list, 0, sizeof *list);

    if(!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        elog("libusb doesn't handle hotplug; devices may not be detected\n");
        return false;
    }

    list->ctx = ctx;
    libusb_hotplug_register_callback(ctx,
        /* Both arriving and departing devices */
        LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
        /* Perform an initial enumeration right now */
        LIBUSB_HOTPLUG_ENUMERATE,
        LIBUSB_HOTPLUG_MATCH_ANY, /* vendorId */
        LIBUSB_HOTPLUG_MATCH_ANY, /* productId */
        LIBUSB_HOTPLUG_MATCH_ANY, /* deviceClass */
        handle_hotplug, list, &list->hotplug_handle);

    return true;
}

void fxlink_device_list_refresh(struct fxlink_device_list *list)
{
    for(int i = 0; i < list->count; i++) {
        struct fxlink_device *fdev = &list->devices[i];
        /* Finish analysis */
        if(fdev->calc && fdev->status == FXLINK_FDEV_STATUS_PENDING)
            fxlink_device_analysis_2(fdev);
    }
}

bool fxlink_device_list_interrupt(struct fxlink_device_list *list)
{
    bool still_running = false;

    for(int i = 0; i < list->count; i++) {
        struct fxlink_device *fdev = &list->devices[i];
        fxlink_device_interrupt_transfers(fdev);

        still_running |= fdev->comm && fdev->comm->tr_bulk_IN;
        still_running |= fdev->comm && fdev->comm->tr_bulk_OUT;
    }

    return still_running;
}

void fxlink_device_list_stop(struct fxlink_device_list *list)
{
    if(!list->ctx)
        return;

    libusb_hotplug_deregister_callback(list->ctx, list->hotplug_handle);

    /* Now free the device list proper */
    for(int i = 0; i < list->count; i++)
        fxlink_device_cleanup(&list->devices[i]);

    free(list->devices);
    memset(list, 0, sizeof *list);
}

//---
// Simplified device enumeration
//---

struct fxlink_device *fxlink_device_find(libusb_context *ctx,
    struct fxlink_filter const *filter)
{
    libusb_device **list = NULL;
    int count = libusb_get_device_list(ctx, &list);
    struct fxlink_device *fdev = NULL;

    if(count < 0) {
        elog("libusb_get_device_list() failed with error %d\n", count);
        return NULL;
    }

    /* Look for any suitable calculator */
    for(int i = 0; i < count; i++) {
        libusb_device *dp = list[i];
        struct libusb_device_descriptor dc;

        libusb_get_device_descriptor(dp, &dc);
        if(!is_casio_calculator(dc.idVendor, dc.idProduct))
            continue;

        /* Since we found a calculator, make the fdev and check further */
        fdev = calloc(1, sizeof *fdev);
        if(!fdev) {
            elog("failed to allocate device structure: %m\n");
            continue;
        }

        libusb_ref_device(dp);
        fdev->dp = dp;
        fdev->status = FXLINK_FDEV_STATUS_PENDING;
        fdev->busNumber = libusb_get_bus_number(dp);
        fdev->deviceAddress = libusb_get_device_address(dp);
        fdev->idVendor = dc.idVendor;
        fdev->idProduct = dc.idProduct;
        fdev->calc = NULL;
        fdev->comm = NULL;
        fxlink_device_analysis_1(fdev, true);
        fxlink_device_analysis_2(fdev);

        if(fdev->status == FXLINK_FDEV_STATUS_IDLE) {
            /* Check the filter */
            struct fxlink_filter properties;
            fxlink_device_get_properties(fdev, &properties);

            /* Success: return that device (no interfaces claimed) */
            if(fxlink_filter_match(&properties, filter))
                break;
        }

        /* Failure: free it and try the next one */
        fxlink_device_cleanup(fdev);
        free(fdev);
        fdev = NULL;
    }

    libusb_free_device_list(list, true);
    return fdev;
}

struct fxlink_device *fxlink_device_find_wait(libusb_context *ctx,
    struct fxlink_filter const *filter, delay_t *delay)
{
    while(true) {
        struct fxlink_device *fdev = fxlink_device_find(ctx, filter);
        if(fdev)
            return fdev;
        if(delay_cycle(delay))
            return NULL;
    }
}
