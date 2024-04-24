//---
//	fxgxa:fxgxa - Main interfaces
//---

#ifndef FX_FXGXA
#define FX_FXGXA

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <g1a.h>
#include <g3a.h>

/* In this file, many functions accept either a [struct g1a] or a [struct g3a]
   as their argument. In this case, the argument is noted [void *gxa]. */


/*
**  Header dumping (dump.c)
*/

/* dump(): Print the detailed header fields of a file
   This function takes as argument the full file loaded into memory and the
   size of the file. It does various printing to stdout as main job.

   @gxa   Full file data
   @size  Size of the file */
void dump(void *gxa, size_t size);


/*
**  Header manipulation (edit.c)
*/

/* sign(): Sign header by filling fixed fields and checksums
   This function fills the fixed fields and various checksums of a g1a file. To
   do this it accesses some of the binary data. To set the user-customizable
   field, use the edit_*() functions. (The value of the customizable fields
   does not influence the checksums so it's okay to not call this function
   afterwards.)

   @g1a   Header to sign
   @size  Size of raw file data */
void sign(void *gxa, size_t size);

/* edit_*(): Set various fields */

void edit_name     (void *gxa, const char *name);
void edit_internal (void *gxa, const char *internal);
void edit_version  (void *gxa, const char *version);
void edit_date     (void *gxa, const char *date);

/* edit_g1a_icon(): Set monochrome icon of a g1a header
   The icon parameter must be loaded in 1-bit bitmap format. */
void edit_g1a_icon(struct g1a *header, uint8_t const *mono);

/* edit_g3a_icon(): Set one of the color icons of a g3a header
   The icon must be loaded in RGB565 16-bit format. */
void edit_g3a_icon(struct g3a *header, uint16_t const *icon, bool selected);


/*
**  Utility functions (util.c)
*/

/* is_g1a(), is_g3a(): Check file type */
bool is_g1a(void *gxa);
bool is_g3a(void *gxa);

#define G1A(gxa) ((struct g1a *)(gxa))
#define G3A(gxa) ((struct g3a *)(gxa))

/* word_sum(): Sum of big-endian words at some offset in a file
   This is used for the third checksum.

   @gxa     File whose checksum is requested
   @offset  File offset to start counting
   @words   Number of words to read
   @size    Size of file (in case the region overflows) */
uint16_t word_sum(void const *gxa, size_t offset, int words, size_t size);

/* checksum_g1a(): Word sum of 8 big-endian shorts at 0x300 */
uint16_t checksum_g1a(struct g1a const *g1a, size_t size);

/* checksum_g3a(): Word sum of 8 big-endian shorts at 0x7100 */
uint16_t checksum_g3a(struct g3a const *g3a, size_t size);

/* checksum_g3a_2(): Sum of ranges 0...0x20 + 0x24...EOF-4 (in-out) */
uint32_t checksum_g3a_2(struct g3a const *g3a, size_t size);

/* default_output(): Calculate default output file name
   This function computes a default file name by replacing the extension of
   @name (if it exists) or adding one. The extension is specified as a suffix,
   usually in the form ".ext".

   The resulting string might be as long as the length of @name plus that of
   @suffix (plus one NUL byte); the provided buffer must point to a suitably-
   large allocated space.

   @name    Input file name
   @suffix  Suffix to add or replace @name's extension with
   @output  Output file name */
void default_output(const char *name, const char *suffix, char *output);

/* default_internal(): Calculate default internal name
   This function determines a default internal name, which is '@' followed by
   at most size-1 uppercase letters taken from the application name. The buffer
   must have [size+1] bytes reserved.

   @name    Application name
   @output  Internal name string (9 bytes) */
void default_internal(const char *name, char *output, size_t size);


/*
**  File manipulation (file.c)
*/

/* load_gxa(): Load a g1a/g3a file into memory
   This function loads @filename into a dynamically-allocated buffer and
   returns the address of that buffer; it must be free()'d after use. When
   loading the file, if @size is not NULL, it receives the size of the file.
   On error, load() prints a message an stderr and returns NULL. The header
   is inverted before this function returns.

   @filename  File to load
   @size      If non-NULL, receives the file size
   Returns a pointer to a buffer with loaded data, or NULL on error. */
void *load_gxa(const char *filename, size_t *size);

/* load_binary(): Load a binary file into memory
   This function operates like load_gxa() but reserves space for an empty
   header. The header is initialized with all zeros.

   @filename     File to load
   @size         If non-NULL, receives the file size (with header/footer)
   @header_size  Extra room to add as header
   @footer_size  Extra room to add as footer
   Returns a pointer to a buffer with loaded data, or NULL on error. */
void *load_binary(const char *filename, size_t *size, int header_size,
   int footer_size);

/* save_gxa(): Save a g1a/g3a file to disk
   This functions creates @filename, then writes a header and a chunk of raw
   data to it. Since it temporarily inverts the header to comply with Casio's
   obfuscated format, it needs write access to @g1a. Returns non-zero on error.

   @filename  File to write (it will be overridden if it exists)
   @gxa       G1A/G3A data to write
   @size      Size of data
   Returns zero on success and a nonzero error code otherwise. */
int save_gxa(const char *filename, void *gxa, size_t size);


/*
**  Icon management (icon.c)
*/

/* icon_load(): Load a PNG image into a RGB888 array
   This function loads a PNG image into an RGB888 buffer. If the image is not a
   PNG image or a reading error occurs, this functions prints an error message
   on stderr and returns NULL.

   @filename  PNG file to load
   @width     If non-NULL, receives image width
   @height    If non-NULL, receives image height
   Returns a pointer to a free()able buffer with loaded data, NULL on error. */
uint8_t *icon_load(const char *filename, int *width, int *height);

/* icon_save(): Save an RGB888 array to a PNG image.
   @filename  Target filename
   @input     A 24-bit RGB888 array
   @width     Width of input (should have no gaps)
   @height    Height of input
   Returns non-zero on error. */
int icon_save(const char *filename, uint8_t *input, int width, int height);

/* icon_conv_24to1(): Convert RGB888 to 1-bit monochrome array
   Rows are byte-padded (ie. width is rounded up to a multiple of 8). Returns
   newly-allocated memory, NULL on error. */
uint8_t *icon_conv_24to1(uint8_t const *rgb24, int width, int height);

/* icon_conv_1to24(): Convert a 1-bit monochrome array to RGB888 */
uint8_t *icon_conv_1to24(uint8_t const *mono, int width, int height);

/* icon_conv_24to16(): Convert RGB888 to big-endian RGB565 */
uint16_t *icon_conv_24to16(uint8_t const *rgb24, int width, int height);

/* icon_conv_16to24(): Convert big-endian RGB565 to RGB888 */
uint8_t *icon_conv_16to24(uint16_t const *rgb16be, int width, int height);

/* icon_print_1(): Show a 1-bit image on stdout (ASCII art) */
void icon_print_1(uint8_t const *mono, int width, int height);

/* icon_print_16(): Show a 16-bit image on stdout (RGB ANSI escape codes) */
void icon_print_16(uint16_t const *rgb16be, int width, int height);

#endif /* FX_FXGXA */
