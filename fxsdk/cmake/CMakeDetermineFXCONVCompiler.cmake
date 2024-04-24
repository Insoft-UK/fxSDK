set(FXCONV_COMPILER_NAME $ENV{FXCONV_PATH})
if(NOT FXCONV_COMPILER_NAME)
  set(FXCONV_COMPILER_NAME "fxconv")
endif()

find_program(FXCONV_COMPILER_PATH "${FXCONV_COMPILER_NAME}")
if(FXCONV_COMPILER_PATH STREQUAL "FXCONV_COMPILER_PATH-NOTFOUND")
  message(FATAL_ERROR "'${FXCONV_COMPILER}' not found!")
endif()

set(CMAKE_FXCONV_COMPILER "${FXCONV_COMPILER_PATH}")
set(CMAKE_FXCONV_COMPILER_ENV_VAR "FXCONV_PATH")
set(CMAKE_FXCONV_OUTPUT_EXTENSION ".o")

# Save these results
configure_file(
  "${CMAKE_CURRENT_LIST_DIR}/CMakeFXCONVCompiler.cmake.in"
  "${CMAKE_PLATFORM_INFO_DIR}/CMakeFXCONVCompiler.cmake")
