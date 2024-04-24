//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//

#include <fxlink/protocol.h>
#include <fxlink/logging.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

bool fxlink_message_is_apptype(struct fxlink_message const *msg,
    char const *application, char const *type)
{
    return memchr(application, '\0', 17) != NULL
        && memchr(type, '\0', 17) != NULL
        && !strncmp(msg->application, application, 16)
        && !strncmp(msg->type, type, 16);
}

void fxlink_message_free(struct fxlink_message *message, bool free_data)
{
    if(free_data)
        free(message->data);
    free(message);
}

//---
// Image decoding
//---

static int img_bytes_per_row(int format, int width)
{
    if(format == FXLINK_MESSAGE_IMAGE_RGB565)
        return 2 * width;
    if(format == FXLINK_MESSAGE_IMAGE_MONO)
        return (width + 7) >> 3;
    if(format == FXLINK_MESSAGE_IMAGE_GRAY)
        return 2 * ((width + 7) >> 3);

    return 0;
}

static struct fxlink_message_image_raw *decode_rgb565(void const *pixels,
    int size, struct fxlink_message_image_raw *raw)
{
    int bpr = img_bytes_per_row(FXLINK_MESSAGE_IMAGE_RGB565, raw->width);

    for(int y = 0; y < raw->height; y++) {
        void const *row = pixels + y * bpr;

        for(int x = 0; x < raw->width; x++) {
            /* Don't read past the read buffer if the image is incomplete */
            void const *input = row + 2 * x;
            uint16_t color = 0;
            if(input - pixels + 2 <= size) color = *(uint16_t *)input;

            color = ((color & 0xff00) >> 8) | ((color & 0x00ff) << 8);

            raw->data[y][3*x+0] = (color >> 11) << 3;
            raw->data[y][3*x+1] = ((color >> 5) & 0x3f) << 2;
            raw->data[y][3*x+2] = (color & 0x1f) << 3;
        }
    }
    return raw;
}

static struct fxlink_message_image_raw *decode_mono(void const *pixels,
    int size, struct fxlink_message_image_raw *raw)
{
    int bpr = img_bytes_per_row(FXLINK_MESSAGE_IMAGE_MONO, raw->width);

    for(int y = 0; y < raw->height; y++) {
        void const *row = pixels + y * bpr;

        for(int x = 0; x < raw->width; x++) {
            /* Don't read past the read buffer if the image is incomplete */
            void const *input = row + (x >> 3);
            int byte = 0;
            if(input - pixels + 1 <= size) byte = *(uint8_t *)input;
            int color = (byte & (0x80 >> (x & 7))) ? 0 : 255;

            raw->data[y][3*x+0] = color;
            raw->data[y][3*x+1] = color;
            raw->data[y][3*x+2] = color;
        }
    }
    return raw;
}

static struct fxlink_message_image_raw *decode_gray(void const *pixels,
    int size, struct fxlink_message_image_raw *raw)
{
    int bpr = img_bytes_per_row(FXLINK_MESSAGE_IMAGE_MONO, raw->width);

    for(int k = 0; k < 2 * raw->height; k++) {
        void const *row = pixels + k * bpr;
        int y = k % raw->height;

        for(int x = 0; x < raw->width; x++) {
            /* Don't read past the read buffer if the image is incomplete */
            void const *input = row + (x >> 3);
            int byte = 0;
            if(input - pixels + 1 <= size) byte = *(uint8_t *)input;

            int color = (byte & (0x80 >> (x & 7)));
            /* Everything is inverted */
            if(!color) color = (k >= raw->height) ? 0xaa : 0x55;
            else color = 0x00;

            raw->data[y][3*x+0] += color;
            raw->data[y][3*x+1] += color;
            raw->data[y][3*x+2] += color;
        }
    }
    return raw;
}

struct fxlink_message_image_raw *fxlink_message_image_decode(
    struct fxlink_message const *msg)
{
    if(!fxlink_message_is_fxlink_image(msg)
    && !fxlink_message_is_fxlink_video(msg))
        return NULL;

    struct fxlink_message_image_header *img = msg->data;
    void *pixels = msg->data + sizeof *img;
    int pixels_size = msg->size - sizeof *img;

    /* Determine the expected data size */
    int bytes_per_row = img_bytes_per_row(img->pixel_format, img->width);
    int expected_size = img->height * bytes_per_row;

    if(pixels_size < expected_size)
        wlog("image has %d bytes but needs %d, it will be incomplete\n",
            pixels_size, expected_size);
    if(pixels_size > expected_size)
        wlog("image has %d bytes but only needs %d, dropping extra\n",
            pixels_size, expected_size);

    /* Allocate memory for the decoded pixels */
    struct fxlink_message_image_raw *raw = malloc(sizeof *raw);
    uint8_t **data = calloc(img->height, sizeof(uint8_t *));
    if(!raw || !data)
        goto alloc_error;
    for(size_t i = 0; i < img->height; i++) {
        data[i] = calloc(img->width, 3);
        if(!data[i])
            goto alloc_error;
    }

    raw->width = img->width;
    raw->height = img->height;
    raw->data = data;

    if(img->pixel_format == FXLINK_MESSAGE_IMAGE_RGB565)
        return decode_rgb565(pixels, pixels_size, raw);
    if(img->pixel_format == FXLINK_MESSAGE_IMAGE_MONO)
        return decode_mono(pixels, pixels_size, raw);
    if(img->pixel_format == FXLINK_MESSAGE_IMAGE_GRAY)
        return decode_gray(pixels, pixels_size, raw);

    elog("unknown pixel format: %d\n", img->pixel_format);
    fxlink_message_image_raw_free(raw);
    return NULL;

alloc_error:
    elog("cannot allocate memory for decoded image: %m\n");
    fxlink_message_image_raw_free(raw);
    return NULL;
}

void fxlink_message_image_raw_free(struct fxlink_message_image_raw *raw)
{
    if(raw->data){
        for(int i = 0; i < raw->height; i++)
            free(raw->data[i]);
    }
    free(raw->data);
    free(raw);
}

//---
// Tools for crafting and receiving messages
//---

/* Check whether a header is valid. It's important for this function to be as
   strict as possible so that picking up communication in the wrong spot
   doesn't get out of control by recognizing messages everywhere. */
static bool header_valid(struct fxlink_message const *msg)
{
    if(msg->version == 0x00000100) {
        /* Transfer size larger than 16 MB: impossible for so many reasons */
        if(msg->transfer_size > 0xffffff)
            return false;
        /* Non-printable characters in application/type names */
        for(size_t i = 0; i < sizeof msg->application; i++) {
            if(msg->application[i] && !isprint(msg->application[i]))
                return false;
        }
        for(size_t i = 0; i < sizeof msg->type; i++) {
            if(msg->type[i] && !isprint(msg->type[i]))
                return false;
        }
        return true;
    }
    return false;
}

struct fxlink_transfer *fxlink_transfer_make_IN(void *data, int size)
{
    int header_size = FXLINK_MESSAGE_HEADER_SIZE;

    if(size < header_size) {
        elog("cannot read message: header too short (%d < %zu bytes)\n",
            size, header_size);
        return NULL;
    }

    struct fxlink_transfer *tr = calloc(1, sizeof *tr);
    if(!tr) {
        elog("cannot allocate transfer structure: %m\n");
        return NULL;
    }

    memcpy(&tr->msg, data, header_size);
    if(!header_valid(&tr->msg)) {
        elog("cannot read message: invalid header (%d bytes dropped)\n",
            size);
        free(tr);
        return NULL;
    }

    tr->msg.data = malloc(tr->msg.size);
    tr->direction = FXLINK_TRANSFER_IN;
    tr->processed_size = 0;
    tr->own_data = true;

    if(!tr->msg.data) {
        elog("cannot allocate buffer for %d bytes\n", tr->msg.size);
        free(tr);
        return NULL;
    }

    fxlink_transfer_receive(tr, data + header_size, size - header_size);
    return tr;
}

struct fxlink_message *fxlink_transfer_finish_IN(struct fxlink_transfer *tr)
{
    if(tr->direction != FXLINK_TRANSFER_IN || !fxlink_transfer_complete(tr))
        return NULL;

    struct fxlink_message *msg = malloc(sizeof *msg);
    if(!msg) {
        elog("cannot allocate message structure: %m\n");
        return NULL;
    }

    /* Shallow copy the malloc()'d data pointer */
    assert(tr->own_data && "Can't shallow copy data if we don't own it!");
    memcpy(msg, &tr->msg, sizeof *msg);
    free(tr);
    return msg;
}

void fxlink_transfer_receive(struct fxlink_transfer *tr, void *data, int size)
{
    int remaining_size = tr->msg.size - tr->processed_size;
    if(size > remaining_size) {
        elog("message too long, dropping %d bytes\n", size - remaining_size);
        size = remaining_size;
    }

    memcpy(tr->msg.data + tr->processed_size, data, size);
    tr->processed_size += size;
}

struct fxlink_transfer *fxlink_transfer_make_OUT(char const *application,
    char const *type, void const *data, int size, bool own_data)
{
    struct fxlink_transfer *tr = calloc(1, sizeof *tr);
    if(!tr)
        return NULL;

    tr->msg.version = 0x00000100;
    tr->msg.size = size;
    tr->msg.transfer_size = 0;
    strncpy(tr->msg.application, application, 16);
    strncpy(tr->msg.type, type, 16);
    tr->msg.data = (void *)data;
    tr->direction = FXLINK_TRANSFER_OUT;
    tr->processed_size = -1;
    tr->own_data = own_data;
    return tr;
}

bool fxlink_transfer_complete(struct fxlink_transfer const *tr)
{
    return tr->processed_size >= (int)tr->msg.size;
}

void fxlink_transfer_free(struct fxlink_transfer *tr)
{
    if(tr->own_data)
        free(tr->msg.data);
    free(tr);
}
