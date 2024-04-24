//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//
// fxlink.devices: Device management and state tracking
//
// This module provides definitions for fxlink's view of devices and their
// states. There are three "tiers" of devices depending on how much fxlink
// interacts with them:
//
// - Every connected USB device is inspected and given a `struct fxlink_device`
//   which track metadata like vendor and product IDs. A device at this level
//   isn't very useful so fxlink won't do anything with them.
//
// - Devices with the vendor and product IDs of CASIO calculators are opened;
//   their configuration descriptor is analyzed to determine what interfaces
//   they offer, and some communication is performed to retrieve information
//   like the serial number of the calculator. This information is stored in a
//   `struct fxlink_calc` structure linked in the `calc` field of the main
//   device structure. Only after this stage can device filters be applied.
//
// The key operation to do next is claiming an interface so communication can
// start. fxlink devices can be used for manual communication using the device
// pointer `fdev->dp` and the device handle `fdev->dh`. This is useful for
// communication with arbitrary interfaces like the CESG502 interface used by
// CASIO's Comm syscalls. However, when gint is used and the fxlink-specific
// interface (protocol) is opened, this header provides more automatic tools.
//
// - When calculators expose an fxlink interface, that interface can be claimed
//   with fxlink_device_claim_fxlink(). This module finds relevant endpoints,
//   allocates transfer buffers/utilities, and exposes a higher-level message-
//   based interface. Associated data is stored in a `struct fxlink_comm`
//   structure linked in the `comm` field of the main device structure.
//
// In addition to handling single devices, this header provides two types of
// tools to find devices:
//
// 1. Tracking tools, which are used to watch connected devices in real-time.
//    These are asynchronous/non-blocking, keep track of what devices we've
//    seen before and overall are the most flexible. These are used by the TUI.
//
// 2. "Simplified enumeration" functions which simply search for a single
//    device matching a filter. These are relevant when handling multiple
//    connections are not a concern, and used eg. by legacy interactive mode.
//---

#pragma once
#include <fxlink/protocol.h>
#include <fxlink/filter.h>
#include <libusb.h>
#include <poll.h>

/* Device information tracked for every USB device. */
struct fxlink_device {
    /* libusb device pointer. This is libusb_ref_device()'d for the entire
       lifetime of the structure (either managed by the device list or obtained
       by simplified enumeration and freed by fxlink_device_cleanup()). */
    libusb_device *dp;
    /* libusb device handle (NULL when the device is not open) */
    libusb_device_handle *dh;
    /* Device status (an FDEV_STATUS_* enumerated value) */
    uint8_t status;

    /* Standard USB information */
    short busNumber;
    short deviceAddress;
    uint16_t idVendor;
    uint16_t idProduct;

    /* Calculator data. This field is present whenever a device is a CASIO
       calculator. Information can be accessed freely. */
    struct fxlink_calc *calc;
    /* Communication data for the fxlink interface. This field is present when
       calculators have an fxlink interface. It can be used when not NULL but
       note that the device status might still be IGNORED (if filtered out) or
       ERROR (if communication errors occurs) in which case communication might
       not be allowed. */
    struct fxlink_comm *comm;
};

enum {
    /* The device has just been connected to the host, and we have yet to
       inspect it or open it. We don't know yet whether there are any supported
       interfaces. */
    FXLINK_FDEV_STATUS_PENDING,
    /* The device is ignored by fxlink. This is either because it's not a CASIO
       calculator or because it was excluded by a device filter. The device
       might have been opened (to find out the serial number) but it is now
       closed and no interfaces are claimed. */
    FXLINK_FDEV_STATUS_IGNORED,
    /* The device could not be used due to an error: access denied, interface
       already claimed, transient errors, etc. */
    FXLINK_FDEV_STATUS_ERROR,
    /* The device is a calculator and it's not ignored, but no interfaces have
       been claimed yet. This header only sets the status to CONNECTED when
       using an fxlink interface, but that status is mostly cosmetic and it's
       entirely possible to manually claim and use an interface (eg. CESG502)
       while the device is in the IDLE state. The `comm` field may or may not
       be present depending on whether there is an fxlink interface. */
    FXLINK_FDEV_STATUS_IDLE,
    /* The device is a calculator, is not ignored, has an fxlink interface and
       that interface was claimed. The `comm` field is non-NULL and can be
       checked to be determine whether communication is going on. */
    FXLINK_FDEV_STATUS_CONNECTED,
};

/* Return a string representation of the device's bus number and device
   address, which make up a human-readable a locally unique device ID. The
   address of a static string is returned. */
char const *fxlink_device_id(struct fxlink_device const *fdev);

/* Return a string representation of the current status. */
char const *fxlink_device_status_string(struct fxlink_device const *fdev);

/* Analyze a CASIO calculator device to reach "calculator" tier.

   These functions can be called on CASIO calculators (idVendor 07cf, idProduct
   6101 or 6102). They determine available interfaces, open the device, and
   initialize the `calc` field with information about the device's interfaces.
   If an fxlink interface is found, they also initialize the `comm` field with
   static information (but doesn't claim the interface).

   After analysis, either the device status is ERROR, or it is IDLE and
   interfaces are ready to be claimed.

   fxlink_device_analysis_1() is generally called in an event handling context,
   so its capabilities are limited. fxlink_device_analysis_2() is called after
   event handling finishes to finalize analysis. Both calls are managed by the
   device list or simplified enumeration functions so direct calls are normally
   not needed. `quiet` suppresses logs. */
void fxlink_device_analysis_1(struct fxlink_device *fdev, bool quiet);
void fxlink_device_analysis_2(struct fxlink_device *fdev);

/* Determine the filter properties of a calculator. This can be used for any
   device at the calculator tier. This function only reads the device structure
   and does not communicate. */
void fxlink_device_get_properties(struct fxlink_device const *fdev,
    struct fxlink_filter *properties);

/* Device information tracked for CASIO calculators. */
struct fxlink_calc {
    /* System running on the calculator (a CALC_SYSTEM_* enumerated value) */
    uint8_t system;
    /* Number of interfaces */
    uint8_t interface_count;
    /* fxlink interface number (-1 if not found) */
    int8_t fxlink_inum;
    /* List of interface classes (interface_count elements). Each element is a
       two-byte value, MSB being class and LSB subclass. */
    uint16_t *classes;
    /* Serial number (obtained with string descriptor SETUP request) */
    char *serial;
};

enum {
    /* The device is running an unidentified system. */
    FXLINK_CALC_SYSTEM_UNKNOWN,
    /* The device is using LINK app's SCSI (Mass Storage) interface. */
    FXLINK_CALC_SYSTEM_LINKSCSI,
    /* The device is using the OS' native bulk interface with Comm syscalls. */
    FXLINK_CALC_SYSTEM_CESG502,
    /* The device is using gint's USB driver. */
    FXLINK_CALC_SYSTEM_GINT,
};

/* Return a string representation of the calc determined system. */
char const *fxlink_device_system_string(struct fxlink_device const *fdev);

/* Device state tracked for communication targets. */
struct fxlink_comm {
    /* Whether the fxlink interface could be claimed */
    bool claimed;
    /* Endpoints of the fxlink interface */
    uint8_t ep_bulk_IN;
    uint8_t ep_bulk_OUT;

    /* Current IN transfer */
    struct libusb_transfer *tr_bulk_IN;
    /* IN transfer buffer and its size */
    uint8_t *buffer_IN;
    int buffer_IN_size;
    /* fxlink message construction for the IN transfer */
    struct fxlink_transfer *ftransfer_IN;
    /* Cancellation flag */
    bool cancelled_IN;
    /* Completed transfer objects (to be checked by user every frame) */
    struct fxlink_message *message_IN;

    /* Current OUT transfer */
    struct libusb_transfer *tr_bulk_OUT;
    /* fxlink message construction for the OUT transfer */
    struct fxlink_transfer *ftransfer_OUT;
    /* Cancellation flag */
    bool cancelled_OUT;
};

/* Check whether the device is ready to have interfaces claimed. This function
   only checks that the device is a calculator and could be opened; it doesn't
   guarantee that claiming the interfaces will succeed. This function returns
   true even after an interface has been claimed since multiple interfaces can
   be claimed at the same time. */
bool fxlink_device_ready_to_connect(struct fxlink_device const *fdev);

/* Check whether the device exposes an fxlink interface. */
bool fxlink_device_has_fxlink_interface(struct fxlink_device const *fdev);

/* Claim the fxlink interface (for a device that has one). Returns false and
   sets the status to ERROR on failure. */
bool fxlink_device_claim_fxlink(struct fxlink_device *fdev);

/* Start an IN transfer on the device if none is currently running, so that the
   device structure is always ready to receive data from the calculator. */
bool fxlink_device_start_bulk_IN(struct fxlink_device *fdev);

/* Finish an IN transfer and obtain the completed message. This function should
   be checked every frame as it will return a non-NULL pointer as soon as the
   message is completed and the device will only start a new bulk IN transfer
   until the message is moved out by this function. */
struct fxlink_message *fxlink_device_finish_bulk_IN(
    struct fxlink_device *fdev);

/* Start an OUT transfer on the device. If `own_data` is set, the transfer will
   free(data) when it completes. */
bool fxlink_device_start_bulk_OUT(struct fxlink_device *fdev,
    char const *app, char const *type, void const *data, int size,
    bool own_data);

/* Interrupt any active transfers on the device. */
void fxlink_device_interrupt_transfers(struct fxlink_device *fdev);

/* Clean everything that needs to be cleaned for the device to be destroyed.
   This closes the libusb device handle, frees dynamically allocated memory,
   etc. This function finishes immediately so no transfers must be running
   when it is call; use fxlink_device_interrupt_transfers() first. */
void fxlink_device_cleanup(struct fxlink_device *fdev);

//---
// Polled file descriptor tracking
//
// fxlink has some asynchronous main loops where events from libusb are mixed
// with user input. We poll libusb file descriptors, along with other event
// sources such as stdin, to listen to all event sources at the same time. This
// is possible on Linux because libusb exposes its active file descriptors. The
// following utility is a wrapper to track them, built around libusb's pollfd
// notification API.
//---

/* Tracker for libusb file descriptors to be polled in a main loop. */
struct fxlink_pollfds {
    /* libusb context to be tracked (must remain constant) */
    libusb_context *ctx;
    /* Array of file descriptors currently being polled */
    struct pollfd *fds;
    /* Number of elements in `fds` */
    int count;
};

/* Start tracking file descriptors for a given context. This sets up notifiers
   so tracker->fds and tracker->count will be updated automatically every time
   libusb events are processed. */
void fxlink_pollfds_track(struct fxlink_pollfds *tracker, libusb_context *ctx);

/* Stop tracking file descriptors and free tracker's resources. This must be
   called before the tracker structure gets destroyed. */
void fxlink_pollfds_stop(struct fxlink_pollfds *tracker);

//---
// Device tracking
//
// For real-time interactions and device detection it is useful to keep an eye
// on connected devices, process incoming devices as well as properly free
// resources for disconnected devices. libusb has a hotplug notification
// mechanism, which we wrap here into a dynamic device list.
//
// The list stays constantly updated by means of the hotplug notification and
// initializes an fxlink_device structure for every device. It does all the
// "safe" work, ie. everything that doesn't involve actual communication. For
// instance, it fills in the fields of both the fxlink_device and fxlink_calc
// structures, but it doesn't query serial numbers.
//---

/* A dynamically updated list of all connected devices. */
struct fxlink_device_list {
    /* libusb context that we track the devices for */
    libusb_context *ctx;
    /* Callback handle */
    libusb_hotplug_callback_handle hotplug_handle;
    /* Array of connected devices */
    struct fxlink_device *devices;
    /* Number of elements in `devices` */
    int count;
};

/* Start tracking connected devices for the given context. The list will update
   whenever libusb events are processed and new devices will be available (ie.
   in the IDLE state) after calling fxlink_device_list_refresh(). */
bool fxlink_device_list_track(struct fxlink_device_list *list,
    libusb_context *ctx);

/* Refresh devices. This should be called after handling libusb events; it
   finishes analysis for new devices. After calling this function, devices are
   ready to be filtered and interfaces claimed. */
void fxlink_device_list_refresh(struct fxlink_device_list *list);

/* Interrupt transfers on all devices in the list. This is non-blocking;
   returns true if some transfers are still running. Call this in a loop until
   it returns false, while handling libusb events at each iteration. */
bool fxlink_device_list_interrupt(struct fxlink_device_list *list);

/* Stop tracking connected devices, free the device list and disable the
   hotplug notification. Make sure all devices are closed and communications
   stopped before calling this function. */
void fxlink_device_list_stop(struct fxlink_device_list *list);

//---
// Simplified device enumeration
//
// These functions provides a simpler way to find devices. Given a filter they
// will try to find a device that matches, and build a device structure when
// they find one. These are only suitable to open a single device since they
// don't track which fxlink devices have been created.
//---

/* Finds a libusb device that matches the provided filter.

   If one is found, returns a newly-allocated fxlink device. The device will be
   opened and have its device handler in the `comm->dh` field. Additionally, if
   there is an fxlink interface, that interface will be claimed and
   communication tools from this header will be initialized.

   If no matching device is found, NULL is returned; this is a non-blocking
   function. Note that this function will interact with devices to determine
   their property (ie. serial number) even when it returns NULL. */
struct fxlink_device *fxlink_device_find(libusb_context *ctx,
    struct fxlink_filter const *filter);

/* Same as fxlink_device_find(), but waits for the specified delay. */
// TODO: fxlink_device_find_wait(): Use a struct timespec, get rid of delay_t?
struct fxlink_device *fxlink_device_find_wait(libusb_context *ctx,
    struct fxlink_filter const *filter, delay_t *delay);
