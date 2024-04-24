# fxSDK toolchain file for Casio graphing calculators
# Models: Almost every model of the fx-9750G and fx-9860G families
# Target triplet: sh-elf (custom sh3eb-elf supporting sh3 and sh4-nofpu)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR sh)

set(FXSDK_PLATFORM fx)
set(FXSDK_PLATFORM_LONG fx9860G)

set(FXSDK_TOOLCHAIN sh-elf-)
set(CMAKE_C_COMPILER sh-elf-gcc)
set(CMAKE_CXX_COMPILER sh-elf-g++)

set(CMAKE_C_FLAGS_INIT "")
set(CMAKE_CXX_FLAGS_INIT "")

add_compile_options(-mb -ffreestanding -nostdlib -Wa,--dsp)
add_link_options(-nostdlib -Wl,--no-warn-rwx-segments)
link_libraries(-lgcc)
add_compile_definitions(TARGET_FX9860G)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(FXSDK_CMAKE_MODULE_PATH "${FXSDK_CMAKE_MODULE_PATH}")

# Add the fxSDK prefix path to the search
set(FXSDK_PREFIX "$ENV{FXSDK_PREFIX}")
foreach(DIR IN LISTS FXSDK_PREFIX)
  include_directories("${DIR}/include")
  link_directories("${DIR}/lib")
endforeach()

# Determine compiler install path
execute_process(
  COMMAND ${CMAKE_C_COMPILER} --print-file-name=.
  OUTPUT_VARIABLE FXSDK_COMPILER_INSTALL
  OUTPUT_STRIP_TRAILING_WHITESPACE)

# Provide fxSDK sysroot and standard install folders
execute_process(
  COMMAND fxsdk path sysroot
  OUTPUT_VARIABLE FXSDK_SYSROOT
  OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
  COMMAND fxsdk path include
  OUTPUT_VARIABLE FXSDK_INCLUDE
  OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
  COMMAND fxsdk path lib
  OUTPUT_VARIABLE FXSDK_LIB
  OUTPUT_STRIP_TRAILING_WHITESPACE)
