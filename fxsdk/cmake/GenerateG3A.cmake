include(FxsdkUtils)

function(generate_g3a)
  cmake_parse_arguments(G3A "" "TARGET;OUTPUT;NAME;VERSION" "ICONS" ${ARGN})

  # Check arguments

  if(DEFINED G3A_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "generate_g3a: Unrecognized arguments ${G3A_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT DEFINED G3A_TARGET)
    message(FATAL_ERROR "generate_g3a: TARGET argument is required")
  endif()

  list(LENGTH G3A_ICONS G3A_ICONS_LENGTH)
  if(DEFINED G3A_ICONS AND NOT ("${G3A_ICONS_LENGTH}" EQUAL 2))
    message(ERROR "generate_g3a: ICONS expects exactly 2 files")
  endif()

  # Find output file name

  if(DEFINED G3A_OUTPUT)
    get_filename_component(G3A_OUTPUT "${G3A_OUTPUT}" ABSOLUTE
      BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    shell_escape("${G3A_OUTPUT}" G3A_OUTPUT)
  else()
    set(G3A_OUTPUT "${G3A_TARGET}.g3a")
  endif()

  # Compute the set of fxgxa arguments

  set(FXGXA_ARGS "")

  # Empty names are commonly used to avoid having the file name printed over
  # the icon, but using ARGN in cmake_parse_arguments() drops the empty string.
  # Check in KEYWORDS_MISSING_VALUES as a complement.
  if(DEFINED G3A_NAME OR "NAME" IN_LIST G3A_KEYWORDS_MISSING_VALUES)
    if("${G3A_NAME}" STREQUAL "")
      set(G3A_NAME "''")
    endif()
    list(APPEND FXGXA_ARGS "-n" "${G3A_NAME}")
  endif()

  if(DEFINED G3A_VERSION)
    list(APPEND FXGXA_ARGS "--version=${G3A_VERSION}")
  endif()

  if(DEFINED G3A_ICONS)
    list(GET G3A_ICONS 0 G3A_ICON1)
    list(GET G3A_ICONS 1 G3A_ICON2)
    get_filename_component(G3A_ICON1 "${G3A_ICON1}" ABSOLUTE
      BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(G3A_ICON2 "${G3A_ICON2}" ABSOLUTE
      BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    shell_escape("${G3A_ICON1}" G3A_ICON1B)
    shell_escape("${G3A_ICON2}" G3A_ICON2B)
    list(APPEND FXGXA_ARGS "--icon-uns=${G3A_ICON1B}" "--icon-sel=${G3A_ICON2B}")
  endif()

  string(REPLACE "gcc" "objcopy" OBJCOPY "${CMAKE_C_COMPILER}")

  add_custom_command(
    TARGET "${G3A_TARGET}" POST_BUILD
    COMMAND ${OBJCOPY} -O binary -R .bss -R .gint_bss ${G3A_TARGET} ${G3A_TARGET}.bin
    COMMAND "$<IF:$<CONFIG:FastLoad>,,fxgxa;--g3a;${FXGXA_ARGS};${G3A_TARGET}.bin;-o;${G3A_OUTPUT}>"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMAND_EXPAND_LISTS
  )
  if(DEFINED G3A_ICONS)
    set_target_properties("${G3A_TARGET}" PROPERTIES
      LINK_DEPENDS "${G3A_ICON1};${G3A_ICON2}")
  endif()
endfunction()
