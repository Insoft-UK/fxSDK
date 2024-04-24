# Locate the library file and includes

find_library(
  LIBFXLINK_PATH "fxlink"
  HINTS "$ENV{HOME}/.local/lib" "$ENV{FXSDK_PATH}/lib"
)
if(LIBFXLINK_PATH STREQUAL "LIBFXLINK_PATH-NOTFOUND")
  message(SEND_ERROR
    "Could not find libfxlink.a!\n"
    "You can specify the install path with the environment variable "
    "FXSDK_PATH, such as FXSDK_PATH=$HOME/.local")
else()
  get_filename_component(LIBFXLINK_PATH "${LIBFXLINK_PATH}/../.." ABSOLUTE)
  set(LIBFXLINK_LIB "${LIBFXLINK_PATH}/lib/libfxlink.a")
  set(LIBFXLINK_INCLUDE "${LIBFXLINK_PATH}/include")

  message("(libfxlink) Found libfxlink at: ${LIBFXLINK_LIB}")
  message("(libfxlink) Will take includes from: ${LIBFXLINK_INCLUDE}")
endif()

# Find library version

if(NOT EXISTS "${LIBFXLINK_INCLUDE}/fxlink/config.h")
  message(SEND_ERROR
    "No <fxlink/config.h> exists at ${LIBFXLINK_INCLUDE}/fxlink/config.h\n"
    "Is libfxlink installed alongside the headers?")
endif()

execute_process(
  COMMAND sed "s/#define FXLINK_VERSION \"\\([^\"]\\{1,\\}\\)\"/\\1/p; d"
          "${LIBFXLINK_INCLUDE}/fxlink/config.h"
  OUTPUT_VARIABLE LIBFXLINK_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE)
message("(libfxlink) Library version found in header: ${LIBFXLINK_VERSION}")

# Handle find_package() arguments and find dependencies

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibFxlink
  REQUIRED_VARS LIBFXLINK_LIB LIBFXLINK_INCLUDE
  VERSION_VAR LIBFXLINK_VERSION)

find_package(PkgConfig REQUIRED)
pkg_check_modules(libusb REQUIRED libusb-1.0 IMPORTED_TARGET)

# Generate targets

if(LibFxlink_FOUND)
  if(NOT TARGET LibFxlink::LibFxlink)
    add_library(LibFxlink::LibFxlink UNKNOWN IMPORTED)
  endif()

  set_target_properties(LibFxlink::LibFxlink PROPERTIES
    IMPORTED_LOCATION "${LIBFXLINK_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBFXLINK_INCLUDE}")
  target_link_libraries(LibFxlink::LibFxlink INTERFACE PkgConfig::libusb)
endif()
