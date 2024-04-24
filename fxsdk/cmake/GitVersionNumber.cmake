find_package(Git)

function(git_version_number)
  cmake_parse_arguments(GVN "" "GIT_DIR;HASH;SHORT_HASH;TAG_RELATIVE" "" ${ARGN})

  if(NOT Git_FOUND)
    message(ERROR "git_version_number did not find Git")
  endif()

  # Use specified GIT_DIR, or default to .git in current source directory
  if(DEFINED GVN_GIT_DIR)
    set(GIT_DIR "${GVN_GIT_DIR}")
  else()
    set(GIT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.git")
  endif()

  set(BASE "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/GitVersionNumber")
  file(READ "${GIT_DIR}/HEAD" HEAD_TEXT)

  # Determine which file holds the current hash; we'll then track the contents
  # of this file as well as HEAD with a build-time dependency.
  if("${HEAD_TEXT}" MATCHES "^ref: ")
    # HEAD tied to a ref: follow the ref file
    string(REPLACE "ref:" "" HEAD_TEXT "${HEAD_TEXT}")
    string(STRIP "${HEAD_TEXT}" HEAD_TEXT)
    set(GIT_TRACKED_FILE "${GIT_DIR}/${HEAD_TEXT}")
  else()
    # Detached HEAD: follow .git/HEAD
    set(GIT_TRACKED_FILE "${GIT_DIR}/HEAD")
  endif()

  # Create a configure-time dependency on .git/HEAD so that whenever it changes
  # (current branch changes or detached HEAD moves), CMake is re-run to update
  # the file to track if needed.
  configure_file("${GIT_DIR}/HEAD" "${BASE}_HEAD" COPYONLY)

  # Create a configure-time dependency on GIT_TRACKED_FILE so that whenever it
  # changes (current branch moves), this script is re-run to provide new values
  # to the caller (which will likely need them at configure time for
  # configure_file() calls of their own.)
  configure_file("${GIT_TRACKED_FILE}" "${BASE}_HASH" COPYONLY)

  # Extract version hash
  execute_process(
    COMMAND cat "${GIT_TRACKED_FILE}"
    OUTPUT_VARIABLE HASH_VALUE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  if(DEFINED GVN_HASH)
    set("${GVN_HASH}" "${HASH_VALUE}" PARENT_SCOPE)
  endif()

  # Short version hash
  if(DEFINED GVN_SHORT_HASH)
    string(SUBSTRING "${HASH_VALUE}" 0 7 SHORT_HASH_VALUE)
    set("${GVN_SHORT_HASH}" "${SHORT_HASH_VALUE}" PARENT_SCOPE)
  endif()

  # Version from tag
  if(DEFINED GVN_TAG_RELATIVE)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} describe --tag --always "${HASH_VALUE}"
      COMMAND cut -d- -f1-2
      OUTPUT_VARIABLE TAG_VALUE
      OUTPUT_STRIP_TRAILING_WHITESPACE
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    set("${GVN_TAG_RELATIVE}" "${TAG_VALUE}" PARENT_SCOPE)
  endif()
endfunction()
