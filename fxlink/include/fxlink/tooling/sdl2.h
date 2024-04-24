//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//
// fxlink.tooling.sdl2: Utilities based on the SDL2 library
//
// Note: all the functions in this file are "neutralized" if the compile-time
// option FXLINK_DISABLE_SDL2 is set. See <fxlink/config.h.in>.
//---

#pragma once
#include <fxlink/protocol.h>

#ifndef FXLINK_DISABLE_SDL2
#include <SDL2/SDL.h>
#endif

/* Display a raw image on the window. If now window has been opened yet, one is
   created automatically. */
void fxlink_sdl2_display_raw(struct fxlink_message_image_raw const *raw);

/* Handle SDL events. This should be called regularly from the main thread. */
void fxlink_sdl2_handle_events(void);
