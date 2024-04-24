# The compiler command is a bit shaky because we want to dynamically add
# --converters flags but too much user code carelessly sets C/C++ flags for all
# languages globally, meaning our own flags are mostly unusable.
#
# We work around this problem by using CMAKE_FXCONV_COMPILER_ARG1, a special
# variable whose value is automatically inserted as an argument after the
# <CMAKE_FXCONV_COMPILER> placeholder in the COMPILE_OBJECT rule. We then
# modify CMAKE_FXCONV_COMPILER_ARG1 from the fxconv_declare_converters()
# function to supply the --converters flag.

set(CMAKE_FXCONV_COMPILER_ARG1)
set(CMAKE_FXCONV_COMPILE_OBJECT
  "<CMAKE_FXCONV_COMPILER> --toolchain=sh-elf --${FXSDK_PLATFORM} \
   <SOURCE> -o <OBJECT>")
set(CMAKE_FXCONV_INFORMATION_LOADED 1)
