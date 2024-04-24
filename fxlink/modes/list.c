//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//

#include "../fxlink.h"
#include <fxlink/devices.h>
#include <fxlink/filter.h>
#include <fxlink/logging.h>
#include <stdio.h>

static int print_devices(struct fxlink_device_list const *list,
    struct fxlink_filter const *filter)
{
    int total_devices = 0;

    for(int i = 0; i < list->count; i++) {
        struct fxlink_device const *fdev = &list->devices[i];
        if(!fdev->calc)
            continue;

        struct fxlink_filter properties;
        fxlink_device_get_properties(fdev, &properties);
        if(!fxlink_filter_match(&properties, filter))
            continue;

        if(total_devices > 0)
            printf("\n");

        if(fdev->idProduct == 0x6101)
            printf("fx-9860G series (Protocol 7) calculator\n");
        else if(fdev->idProduct == 0x6102)
            printf("fx-CG or G-III series (USB Mass Storage) calculator\n");
        else
            printf("Unknown calculator (idProduct: %04x)\n", fdev->idProduct);

        printf("     Device location:  Bus %d, Port %d, Device %d\n",
            libusb_get_bus_number(fdev->dp),
            libusb_get_port_number(fdev->dp),
            libusb_get_device_address(fdev->dp));
        printf("      Identification:  idVendor: %04x, idProduct: %04x\n",
            fdev->idVendor, fdev->idProduct);
        /* FIXME: This assumes a short path (no hub or dual-device) */
        printf("  Guessed sysfs path:  /sys/bus/usb/devices/%d-%d/\n",
            libusb_get_bus_number(fdev->dp),
            libusb_get_port_number(fdev->dp));

        if(fdev->calc->serial)
            printf("       Serial number:  %s\n", fdev->calc->serial);

        printf("              System:  %s (%s)\n",
            fxlink_device_system_string(fdev),
            fxlink_device_status_string(fdev));

        printf("          Interfaces: ");
        for(int i = 0; i < fdev->calc->interface_count; i++)
            printf(" %02x.%02x",
                fdev->calc->classes[i] >> 8, fdev->calc->classes[i] & 0xff);
        printf("\n");

        printf("          Properties:  ");
        fxlink_filter_print(stdout, &properties);
        printf("\n");

        total_devices++;
    }

    return total_devices;
}

static void discard_logs(int display_fmt, char const *str)
{
    (void)display_fmt;
    (void)str;
}

int main_list(struct fxlink_filter *filter, delay_t *delay,
    libusb_context *ctx)
{
    /* Silence all logs for this mode */
    fxlink_log_set_handler(discard_logs);

    struct fxlink_device_list list;
    struct timeval zero_tv = { 0 };
    fxlink_device_list_track(&list, ctx);

    while(1) {
        libusb_handle_events_timeout(ctx, &zero_tv);
        fxlink_device_list_refresh(&list);

        int n = print_devices(&list, filter);
        if(n > 0)
            break;

        if(delay_cycle(delay)) {
            printf("No%s device found.\n", filter ? " matching" : "");
            break;
        }
    }

    fxlink_device_list_stop(&list);
    return 0;
}
