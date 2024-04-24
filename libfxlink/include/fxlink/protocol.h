//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//
// fxlink.protocol: Basic fxlink-specific communications procotol
//
// This header implements fxlink's run-of-the-mill communications protocol.
// Messages are typed with an application/type pair and their length must be
// announced in the header. In most modes when a message with an application
// field other than "fxlink" is specified, an external executable is invoked,
// which provides a basic form of automation.
//
// Message headers are transferred in little-endian format but some message
// types are defined to have big-endian contents to avoid excessive amounts of
// work on the calculator side.
//---

/* TODO: fxlink protocol: avoid having to specify message length in header */

#pragma once
#include <fxlink/defs.h>

/* Message. The header consists of every field but the `data` pointer, and it
   must arrive entirely within the first transaction for a message to be
   recognized. */
struct fxlink_message {
    /* Protocol version, in format 0x0000MMmm */
    uint32_t version;
    /* Total size of message, in bytes (excluding this header) */
    uint32_t size;
    /* Size of individual transfers (usually 2048 bytes) */
    /* TODO: fxlink protocol: get rid of transfer size field (blank it out) */
    uint32_t transfer_size;

    /* Application name (NUL-padded but might not be NUL-terminated) */
    char application[16];
    /* Message type (NUL-padded but might not be NUL-terminated) */
    char type[16];

    /** End of actual header in protocol **/

    /* Padding for alignment */
    int _padding;
    /* Pointer to message data, with `size` bytes */
    void *data;
};

#define FXLINK_MESSAGE_HEADER_SIZE (offsetof(struct fxlink_message, _padding))

/* Subheader for the built-in image message type. */
struct fxlink_message_image_header {
    /* Image width and height, in pixels */
    uint32_t width;
    uint32_t height;
    /* Pixel format, one of the FXLINK_MESSAGE_IMAGE_* values below */
    int pixel_format;
};

enum {
    /* Image is an array of big-endian uint16_t values with RGB565 format */
    FXLINK_MESSAGE_IMAGE_RGB565,
    /* Image is an array of bits in black-and-white format */
    FXLINK_MESSAGE_IMAGE_MONO,
    /* Image is two consecutive mono arrays, one for light, one for dark */
    FXLINK_MESSAGE_IMAGE_GRAY,
};

/* Format for raw decoded images to be used with other APIs. */
struct fxlink_message_image_raw {
    /* Width and height in pixels */
    int width;
    int height;
    /* Allocated array of `height` pointers, each pointing to `3*width` bytes
       of memory containing the RGB data of pixels from left to right. */
    uint8_t **data;
};

/* Check whether a message has a certain application and type. If type is NULL,
   checks the application only (any type is accepted). */
bool fxlink_message_is_apptype(struct fxlink_message const *message,
    char const *application, char const *type);

/* Check if a message is one of common built-in fxlink messages. */
#define fxlink_message_is_fxlink_text(MESSAGE) \
    fxlink_message_is_apptype(MESSAGE, "fxlink", "text")
#define fxlink_message_is_fxlink_image(MESSAGE) \
    fxlink_message_is_apptype(MESSAGE, "fxlink", "image")
#define fxlink_message_is_fxlink_video(MESSAGE) \
    fxlink_message_is_apptype(MESSAGE, "fxlink", "video")

/* Decode an image message into a raw image structure. */
struct fxlink_message_image_raw *fxlink_message_image_decode(
    struct fxlink_message const *msg);

/* Free a raw image structure. */
void fxlink_message_image_raw_free(struct fxlink_message_image_raw *raw);

/* Free memory associated with a message. If free_data is true, also frees the
   internal data buffer. You should set the flag for messages you received. */
void fxlink_message_free(struct fxlink_message *message, bool free_data);

//---
// Tools for crafting and receiving messages
//---

/* Data for an inbound or outbound transfer in progress. This structure can be
   created as soon as a header has been received or filled. */
struct fxlink_transfer {
    /* Message header and data buffer */
    struct fxlink_message msg;
    /* Transfer direction (FXLINK_TRANSFER_{IN,OUT}) */
    uint8_t direction;
    /* Size of data sent or received so far */
    int processed_size;
    /* Whether we own msg.data and should free(3) it */
    bool own_data;
};

enum {
    /* Transfer is inbound (calculator -> fxlink) */
    FXLINK_TRANSFER_IN,
    /* Transfer is outbound (fxlink -> calculator) */
    FXLINK_TRANSFER_OUT,
};

/* Make an inbound transfer structure starting with the provided data (the data
   obtained in the first IN transaction to be decoded). This function grabs the
   header from the data, allocates a buffer of a suitable size and starts
   saving the message's contents. Returns NULL on error (invalid or incomplete
   header, out of memory, etc). */
struct fxlink_transfer *fxlink_transfer_make_IN(void *data, int size);

/* If the provided IN transfer is finished, extract the message and free the
   transfer (do not fxlink_transfer_free(tr) later!). Returns NULL if the
   transfer isn't finished yet. */
struct fxlink_message *fxlink_transfer_finish_IN(struct fxlink_transfer *tr);

/* Append data to a previously-initialized inbound transfer. */
void fxlink_transfer_receive(struct fxlink_transfer *tr, void *data, int size);

/* Make an outbound transfer structure. */
struct fxlink_transfer *fxlink_transfer_make_OUT(char const *application,
    char const *type, void const *data, int size, bool own_data);

/* Check whether a transfer is complete. */
bool fxlink_transfer_complete(struct fxlink_transfer const *tr);

/* Free a transfer structure and associated data. */
void fxlink_transfer_free(struct fxlink_transfer *tr);
