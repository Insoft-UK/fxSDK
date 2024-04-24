#! /usr/bin/env bash

# These lines are substituted at install time
PREFIX="@CMAKE_INSTALL_PREFIX@"
VERSION="@CMAKE_PROJECT_VERSION@"

R=$(printf "\e[31;1m")
g=$(printf "\e[32m\e[3m")
n=$(printf "\e[0m")
TAG=$(printf "\e[36m<fxSDK>\e[0m")

usage_string=$(cat << EOF
fxSDK version $VERSION
usage: ${R}fxsdk${n} (${R}new${n}|${R}build-fx${n}|${R}build-cg${n}|\
${R}send-fx${n}|${R}send-cg${n}|...) [${g}ARGUMENTS${n}...]

This program is a command-line helper for the fxSDK, a set of tools used in
conjunction with gint to develop add-ins for CASIO fx-9860G and fx-CG 50.

${R}fxsdk new${n} ${g}<FOLDER>${n} [${R}--makefile${n}|${R}--cmake${n}] \
[${g}<NAME>${n}]
  Create a new project in the specified folder. The default build system is
  CMake. Project name can be specified now or in the project files later.

${R}fxsdk${n} (${R}build${n}|${R}build-fx${n}|${R}build-cg${n}) [${R}-c${n}] \
[${R}-s${n}] [${R}--${n}] [${g}<ARGS>${n}...]
  Build the current project for fx-9860G (usually for .g1a add-ins) or fx-CG 50
  (usually for .g3a add-ins). The first form compiles in every existing build
  folder, and configures for both if none exists.

  With -c, reconfigure but do not build (CMake only).
  With -s, also sends the resulting program to the calculator.
  Other arguments are passed to CMake (if using -c) or make (otherwise). You
  can pass -c or -s to CMake/make by specifying --.

${R}fxsdk${n} ${R}build-cg-push${n} [${R}-c${n}] [${R}-s${n}] [${R}--${n}] \
[${g}<ARGS>${n}...]
  Builds the current project for fx-CG 50 for a "fast load" to the calculator.
  This uses Add-In Push by circuit10, which immediately launches the add-in
  without saving it to storage memory, and is much faster than LINK. Options
  are identical to other build commands. Typical workflows will always set -s
  (which requires libusb support in fxlink).

${R}fxsdk${n} (${R}send${n}|${R}send-fx${n}|${R}send-cg${n})
  Sends the target file to the calculator. Uses p7 (which must be installed
  externally) for the fx-9860G, and fxlink for the fx-CG. For the G-III series,
  call fxlink directly instead of this command.

${R}fxsdk${n} ${R}path${n} (${R}sysroot${n}|${R}include${n}|${R}lib${n})
  Prints commonly-used paths in the SuperH sysroot:
    ${R}sysroot${n}    The root folder of the SuperH toolchain and libraries
    ${R}include${n}    Install folder for user's headers
    ${R}lib${n}        Install folder for user's library files
EOF
)

usage() {
  echo "$usage_string"
  exit ${1:-1}
}

error() {
  echo "error:" "$@" >&2
  exit 1
}

fxsdk_new_project() {
  # Generator to use, output folder and project name
  generator="CMake"
  folder=""
  name=""

  # Parse options, then skip to positional arguments
  TEMP=$(getopt -o "" -l "makefile,cmake" -n "$0" -- "$@")
  eval set -- "$TEMP"
  for arg; do case "$arg" in
    "--makefile") generator="Makefile";;
    "--cmake") generator="CMake";;
    *) break;;
  esac; done
  while [[ "$1" != "--" ]]; do shift; done; shift

  if [[ -z "$1" ]]; then
    usage 1
  fi
  if [[ -e "$1" && "$1" != "." ]]; then
    error "$1 exists, I don't dare touch it"
  fi

  # Determine name and internal name
  if [[ ! -z "$2" ]]; then
    NAME=${2::8}
    upper=${2^^}
  else
    cap=${1^}
    NAME=${cap::8}
    upper=${1^^}
  fi
  INTERNAL=@${upper::7}

  # Copy initial files to project folder
  assets="$PREFIX/share/fxsdk/assets"
  mkdir -p "$1"/{,src,assets-fx,assets-cg}

  case "$generator" in
    "Makefile")
      sed -e "s/@NAME@/$NAME/g" -e "s/@INTERNAL@/$INTERNAL/g" \
        "$assets/project.cfg" > "$1/project.cfg"
      cp "$assets/Makefile" "$1"

      mkdir -p "$1"/{assets-fx,assets-cg}/img
      cp -r "$assets"/assets-fx/* "$1"/assets-fx/img/
      cp -r "$assets"/assets-cg/* "$1"/assets-cg/img/
      cp "$assets/icon-cg.xcf" "$1/assets-cg";;

    "CMake")
      cp "$assets/CMakeLists.txt" "$1"
      cp -r "$assets"/assets-fx "$1"/
      cp -r "$assets"/assets-cg "$1"/
      cp "$assets/icon-cg.xcf" "$1/assets-cg";;
  esac

  cp "$assets"/gitignore "$1"/.gitignore
  cp "$assets"/main.c "$1"/src
  cp "$assets"/icon-fx.png "$1"/assets-fx/icon.png
  cp "$assets"/icon-cg-uns.png "$1"/assets-cg/icon-uns.png
  cp "$assets"/icon-cg-sel.png "$1"/assets-cg/icon-sel.png

  echo "Created a new project $NAME (build system: $generator)."
  echo "Type 'fxsdk build-fx' or 'fxsdk build-cg' to compile the program."
}

fxsdk_load_config() {
  grep -E '^ *[a-zA-Z0-9_]+ *=' project.cfg \
  | sed -E 's/^([A-Z_]+)\s*=\s*(.*)/\1="\2"/' \
  | source /dev/stdin
}


fxsdk_build() {
  [[ ! -e build-fx && ! -e build-cg ]]
  none_exists=$?

  if [[ -e build-fx || $none_exists == 0 ]]; then
    echo "$TAG Making into build-fx"
    fxsdk_build_fx "$@"
  fi

  if [[ -e build-cg || $none_exists == 0 ]]; then
    echo "$TAG Making into build-cg"
    fxsdk_build_cg "$@"
  fi
}

fxsdk_build_fx() {
  fxsdk_build_in "fx" "FX9860G" "$@"
}
fxsdk_build_cg() {
  fxsdk_build_in "cg" "FXCG50" "$@"
}
fxsdk_build_cg_push() {
  fxsdk_build_in "cg-push" "FXCG50" "$@"
}

fxsdk_build_in() {
  platform="$1"
  toolchain="$2"
  shift 2

  # Read -s, -c and -- to isolate arguments to CMake and make
  while true; do
    case "$1" in
      "-s") send=1;;
      "-c") configure=1;;
      "--") shift; break;;
      *) break;;
    esac
    shift
  done

  # CMake version; automatically configure
  if [[ -e "CMakeLists.txt" ]]; then
    cmake_extra_args=()
    make_extra_args=()
    if [[ ! -z "$configure" ]]; then
      cmake_extra_args=( "$@" )
    else
      make_extra_args=( "$@" )
    fi

    if [[ ! -e "build-$platform/Makefile" || ! -z "$configure" ]]; then
      platform_args=()
      if [[ "$platform" == *"-push" ]]; then
        platform_args=( "-DCMAKE_BUILD_TYPE=FastLoad" )
      fi
      cmake -B "build-$platform" \
        -DCMAKE_MODULE_PATH="$PREFIX/lib/cmake/fxsdk" \
        -DCMAKE_TOOLCHAIN_FILE="$PREFIX/lib/cmake/fxsdk/$toolchain.cmake" \
        -DFXSDK_CMAKE_MODULE_PATH="$PREFIX/lib/cmake/fxsdk" \
        "${cmake_extra_args[@]}" "${platform_args[@]}"
      if [[ $? != 0 ]]; then
        return 1
      fi
    fi
    if [[ -z "$configure" ]]; then
      make --no-print-directory -C "build-$platform" "${make_extra_args[@]}"
      rc=$?
    fi
  # Makefile version
  else
    make "all-$platform" "$@"
    rc=$?
  fi

  if [[ $rc != 0 ]]; then
    return $rc
  fi

  if [[ ! -z "$send" ]]; then
    fxsdk_send_$platform
  fi
}

fxsdk_send() {
  if [[ -e "build-fx" && ! -e "build-cg" ]]; then
    fxsdk_send_fx
  fi

  if [[ -e "build-cg" && ! -e "build-fx" ]]; then
    fxsdk_send_cg
  fi

  echo "either no or several platforms are targeted, use 'fxsdk send-fx' or"
  echo "fxsdk 'send-cg' to specify which calculator to send to."
}

fxsdk_send_fx() {
  echo "$TAG Installing for fx-9860G using p7"
  if ! command -v p7 >/dev/null 2>&1; then
    echo "error: p7 is not installed or not available"
    return 1
  fi
  g1a_files=$(find -maxdepth 1 -name '*.g1a')
  echo "$TAG Running: p7 send -f ${g1a_files}"
  p7 send -f ${g1a_files}
}

fxsdk_send_cg() {
  echo "$TAG Installing for fx-CG using fxlink"
  if ! command -v fxlink >/dev/null 2>&1; then
    echo "error: fxlink is not installed or not available"
    return 1
  fi
  g3a_files=$(find -maxdepth 1 -name '*.g3a')
  echo "$TAG Running: fxlink -sw ${g3a_files}"
  fxlink -sw ${g3a_files}
}

fxsdk_send_cg-push() {
  echo "$TAG Fast loading to fx-CG using fxlink; open Add-In Push on the calc"
  if ! command -v fxlink >/dev/null 2>&1; then
    echo "error: fxlink is not installed or not available"
    return 1
  fi
  bin_files=$(find "build-cg-push" -maxdepth 1 -name '*.bin')
  echo "$TAG Running: fxlink -pw ${bin_files}"
  fxlink -pw ${bin_files}
}

fxsdk_path() {
  case "$1" in
    "sysroot")
      echo "$PREFIX/share/fxsdk/sysroot";;
    "include")
      echo "$PREFIX/share/fxsdk/sysroot/sh3eb-elf/include";;
    "lib")
      echo "$PREFIX/share/fxsdk/sysroot/sh3eb-elf/lib";;
    "")
      echo "error: no path specified; try 'fxsdk --help'" >&2
      exit 1;;
    *)
      echo "error: unknown path '$1'; try 'fxsdk --help'" >&2
      exit 1;;
  esac
}

# Parse command name

case "$1" in
  # Project creation
  "new")
    fxsdk_new_project "${@:2}";;

  # Project compilation
  "build"|"b")
    fxsdk_build "${@:2}";;
  "build-fx"|"bf"|"bfx")
    fxsdk_build_fx "${@:2}";;
  "build-cg"|"bc"|"bcg")
    fxsdk_build_cg "${@:2}";;
  "build-cg-push"|"bcgp")
    fxsdk_build_cg_push "${@:2}";;

  # Install
  "send"|"s")
    fxsdk_send;;
  "send-fx"|"sf"|"sfx")
    fxsdk_send_fx;;
  "send-cg"|"sc"|"scg")
    fxsdk_send_cg;;

  # Utilities
  "path")
    fxsdk_path "${@:2}";;

  # Misc
  -h|--help|-\?)
    usage 0;;
  --version)
    echo "fxSDK version $VERSION";;
  ?*)
    error "unknown command '$1'"
    exit 1;;
  *)
    usage 0;;
esac
