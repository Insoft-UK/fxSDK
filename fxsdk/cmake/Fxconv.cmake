# For the template of the custom language, see:
# - https://gitlab.kitware.com/cmake/cmake/-/blob/master/Modules/CMakeAddNewLanguage.txt
# - https://stackoverflow.com/questions/59536644/cmake-custom-compiler-linker-language-not-found
enable_language(FXCONV)

function(fxconv_declare_assets)
  cmake_parse_arguments(CONV "WITH_METADATA" "" "" ${ARGN})

  foreach(ASSET IN LISTS CONV_UNPARSED_ARGUMENTS)
    # Declare this source file as an FXCONV object
    set_source_files_properties("${ASSET}" PROPERTIES LANGUAGE FXCONV)

    # Set up a dependency to the local fxconv-metadata.txt
    if(DEFINED CONV_WITH_METADATA)
      get_filename_component(DIR "${ASSET}" DIRECTORY)
      get_filename_component(DIR "${DIR}" ABSOLUTE)
      set(METADATA "${DIR}/fxconv-metadata.txt")
      get_filename_component(METADATA "${METADATA}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
      set_source_files_properties("${ASSET}" PROPERTIES OBJECT_DEPENDS "${METADATA}")
    endif()
  endforeach()
endfunction()

function(fxconv_declare_converters)
  # Get the absolute path for each converter
  foreach(CONVERTER IN LISTS ARGN)
    get_filename_component(CONVERTER_PATH "${CONVERTER}" ABSOLUTE)
    list(APPEND FXCONV_CONVERTERS "${CONVERTER_PATH}")
  endforeach()

  # Record the full list in the parent scope
  set(FXCONV_CONVERTERS "${FXCONV_CONVERTERS}" PARENT_SCOPE)
  # Also push a language flag through the special "ARG1" variable
  set(CMAKE_FXCONV_COMPILER_ARG1 "--converters=${FXCONV_CONVERTERS}"
    PARENT_SCOPE)
endfunction()
