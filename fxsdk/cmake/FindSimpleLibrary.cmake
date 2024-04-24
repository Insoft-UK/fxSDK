function(find_simple_library _library _version_header _version_macro)
  cmake_parse_arguments(CLV "" "PATH_VAR;VERSION_VAR" "OTHER_MACROS" ${ARGN})

  # Find the library path
  execute_process(
    COMMAND fxsdk path lib
    OUTPUT_VARIABLE FXSDK_LIB OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(
    COMMAND fxsdk path include
    OUTPUT_VARIABLE FXSDK_INCLUDE OUTPUT_STRIP_TRAILING_WHITESPACE)

  set(LIB_PATH "${FXSDK_LIB}/${_library}")
  if(NOT EXISTS "${LIB_PATH}")
    message(SEND_ERROR "find_simple_library: Library not found: ${LIB_PATH}")
    return()
  endif()

  if(DEFINED CLV_PATH_VAR)
    set("${CLV_PATH_VAR}" "${LIB_PATH}" PARENT_SCOPE)
  endif()

  # Find the version header
  set(HEADER_PATH "${FXSDK_INCLUDE}/${_version_header}")
  if(NOT EXISTS "${HEADER_PATH}")
    message(SEND_ERROR "find_simple_library: Header not found: ${HEADER_PATH}")
    return()
  endif()

  # Extract the version from the header
  set(SED "s/^\\s*#\\s*define\\s*${_version_macro}\\s*\"(\\S+)\"\\s*$/\\1/p; d")
  execute_process(
    COMMAND sed -E "${SED}" "${HEADER_PATH}"
    OUTPUT_VARIABLE VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)

  if(DEFINED CLV_VERSION_VAR)
    set("${CLV_VERSION_VAR}" "${VERSION}" PARENT_SCOPE)
  endif()

  # Extract other macros from the header
  foreach(MACRO_NAME IN LISTS CLV_OTHER_MACROS)
    set(SED "s/^\\s*#\\s*define\\s*${MACRO_NAME}\\s*(.+)\\s*$/\\1/p; d")
    execute_process(
      COMMAND sed -E "${SED}" "${HEADER_PATH}"
      OUTPUT_VARIABLE MACRO_VALUE OUTPUT_STRIP_TRAILING_WHITESPACE)
    set("${MACRO_NAME}" "${MACRO_VALUE}" PARENT_SCOPE)
  endforeach()
endfunction()
