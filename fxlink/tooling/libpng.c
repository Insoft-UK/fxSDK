//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//

#include <fxlink/tooling/libpng.h>
#include <fxlink/logging.h>

int fxlink_libpng_save_raw(struct fxlink_message_image_raw *raw,
    char const *path)
{
    png_struct *png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
        NULL, NULL, NULL);
    if(!png)
        return elog("failed to write PNG: png_create_write_struct\n");

    png_infop info = png_create_info_struct(png);
    if(!info)
        return elog("failed to write PNG: png_create_info_struct\n");

    FILE *fp = fopen(path, "wb");
    if(!fp) {
        png_destroy_write_struct(&png, &info);
        return elog("failed to open '%s' to write PNG: %m\n", path);
    }

    if(setjmp(png_jmpbuf(png))) {
        fclose(fp);
        png_destroy_write_struct(&png, &info);
        return 1;
    }

    png_init_io(png, fp);
    png_set_IHDR(png, info,
        raw->width, raw->height, 8,
        PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png, info);
    png_write_image(png, raw->data);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    return 0;
}
