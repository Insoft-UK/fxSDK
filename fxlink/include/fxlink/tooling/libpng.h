//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//
// fxlink.tooling.libpng: Utilities based on libpng

#pragma once
#include <fxlink/protocol.h>
#include <png.h>

/* Save a raw image decoded from an fxlink message to a PNG file. Returns
   zero on success. */
int fxlink_libpng_save_raw(struct fxlink_message_image_raw *raw,
    char const *path);
