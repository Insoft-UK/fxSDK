# fxSDK

*Topic on Planète Casio : [fxSDK, un SDK alternatif pour écrire des add-ins](https://www.planet-casio.com/Fr/forums/topic13164-last-fxsdk-un-sdk-alternatif-pour-ecrire-des-add-ins.html)*

The fxSDK is a development kit for CASIO graphing calculators in the fx-9860G and fx-CG series. It provides command-line tools, build systems and a cross-compilation setup for add-ins and libraries. It's designed to be used with the [gint unikernel](/Lephenixnoir/gint) which provides a powerful base runtime to build add-ins from.

This repository only contains the command-line tools of the SDK; in order to program add-ins more components like a [cross-compiler](https://gitea.planet-casio.com/Lephenixnoir/sh-elf-gcc) and usually the [gint kernel](https://gitea.planet-casio.com/Lephenixnoir/gint) are required. See the [Installing](#installing) section for details.

The fxSDK is distributed under the [MIT License](LICENSE).

## Compatibility and support

**Calculators**

The fxSDK (more specifically gint in this case) targets most calculators of the CASIO fx-9860G series, including:

* ![](https://www.planet-casio.com/images/icones/calc/g85.png) (Partial support) SH3-based fx-9860G, fx-9860G SD, similar models with OS 1.xx, and the fx-9860G SDK emulator.
* ![](https://www.planet-casio.com/images/icones/calc/g95.png) SH3-based fx-9750G, fx-9860G II, fx-9860G II SD, and similar models with OS 2.xx.
* ![](https://www.planet-casio.com/images/icones/calc/g75+.png) All SH4-based models with OS 2.xx, including the fx-9750G II and SH4 fx-9860G II.
* ![](https://www.planet-casio.com/images/icones/calc/g35+e2.png) The fx-9750G III and fx-9860G III.

It also supports the calculators of the CASIO fx-CG series:

* ![](https://www.planet-casio.com/images/icones/calc/cg20.png) The original fx-CG 10/20.
* ![](https://www.planet-casio.com/images/icones/calc/g90+e.png) The fx-CG 50.

**Operating systems**

* Linux: officially supported; any distribution should work as long as you can install the dependencies.
* Mac OS: builds consistently but may require slight tweaking due to lack of regular testing.
* Windows: WSL's Ubuntu is officially supported. Windows itself is not supported, but contributions towards it are welcome.

**Programming languages**

* C: Latest GCC support for the language (currently C11/C2X). The standard library is the custom-built [fxlibc](https://gitea.planet-casio.com/Vhex-Kernel-Core/fxlibc/) which supports most of C99. Using another ported libc is possible.
* C++: Latest GCC support for the language (currently C++20/C++23). The standard library is the latest [libstdc++](https://gcc.gnu.org/onlinedocs/libstdc++/).
* Assembly: CASIO calculators run on SuperH processors (mostly the SH4AL-DSP). binutils provides the tools needed to write programs in assembler.
* Others? Stuff that compiles to C is fine (eg. [fxtran for Fortran](https://www.planet-casio.com/Fr/forums/topic17064-1-fxtran-codez-en-fortran-pour-votre-casio.html)). Other GCC targets can work ([the D compiler builds](https://www.planet-casio.com/Fr/forums/topic17037-last-omegafail-dlang-et-gint.html) -- libphobos slightly harder). Languages that target LLVM, like Rust, are definitely out of the question for now as LLVM does not have a SuperH backend.

**Build systems**

The fxSDK uses CMake as the main build system for add-ins and libraries. An older Makefile-based project template is still available and CLI interfaces are officially maintained, but you're expected to manage your build system through updates if you stray from CMake.

## Installing

The following options are all for installing the entire fxSDK including the cross-compiler, libraries, etc. not just this repository.

**Method 1: Using GiteaPC (recommended for beginners)**

[GiteaPC](https://gitea.planet-casio.com/Lephenixnoir/GiteaPC) is a package-manager-like set of scripts to clone, build and install repositories from Planète Casio's Gitea forge. It automates basically the entire process, and is the recommended way to get the fxSDK up and running. See the instructions on the repository's README.

**Method 2: Using the AUR (for Arch Linux-based distributions)**

[Dark Storm](https://www.planet-casio.com/Fr/compte/voir_profil.php?membre=Dark%20Storm) maintains a package repository for Arch called [MiddleArch](https://www.planet-casio.com/Fr/forums/topic16790-1-middlearch-un-depot-communautaire.html) which includes consistently up-to-date versions of the fxSDK.

**Method 3: Manual build (for experienced users)**

You can build the fxSDK and its tools manually by following the instructions in each README file. Please refer to the GiteaPC tutorial for a list of what to install in what order. As a warning: there is quite a lot of stuff (SDK tools, the cross-compiler, a libm, a libc, the libstdc++, the kernel, user libraries and then some) so expect to spend some time installing and updating everything.

## Using the fxSDK: Command-line tools

When developing add-ins with the fxSDK, you mainly interact with command-line tools and the fxSDK's build system. Let's first have a look at the command-line tools. You can get the command-line help for any tool by invoking it without arguments, eg `fxsdk` or `fxgxa`.

*Note: A tool called `fxos` used to live here and has now moved to [its own repository](/Lephenixnoir/fxos).*

**Project management** with `fxsdk`

Use the `fxsdk` command to manage projects. You can create an empty add-in project with `fxsdk new` and a name for a new folder:

```
% fxsdk new MyAddin
Created a new project MyAddin (build system: CMake).
Type 'fxsdk build-fx' or 'fxsdk build-cg' to compile the program.
```

From that folder, you can build the add-in with the `fxsdk build-*` commands:

```bash
# Build the add-in for fx-98600G-series calculators (.g1a):
% fxsdk build-fx
# Build the add-in for fx-CG-series calculators (.g3a):
% fxsdk build-cg
```

You can then send the add-in through your preferred method. Some shortcuts are provided:

* `fxsdk send-fx` will send the g1a file with [p7](https://gitea.planet-casio.com/cake/p7utils) (which is like FA-124/xfer9860) if it's installed. This works for every fx-9860G-series models except the fx-975G0G III and fx-9860G III.
* `fxsdk send-cg` will send the g3a file with fxlink using UDisks2, replicating the process of copying with the file manager. See below for details about fxlink.

The command `fxsdk path` reports the folders in which the important files of the SDK (mainly the cross-compiler) are located.

**G1A/G3A file generation** with `fxgxa`

`fxgxa` is a versatile g1a/g3a file editor that creates, edits and dumps the header of add-ins files. The build system calls it as part of `fxsdk build-*` so you only need to use it directly when you want to inspect existing add-ins.

It supports using and dumping PNG icons of any formats, validating header checksums, repairing broken headers and dumping add-in details. Here are the main commands:

* `fxgxa --g1a|--g3a`: Generate g1a/g3a files
* `fxgxa -e`: Edit g1a/g3a files
* `fxgxa -d`: Dump metadata, checksum, and icon
* `fxgxa -r`: Repair control bytes and checksums for broken files
* `fxgxa -x`: Extract icons into PNG files

`fxgxa` has an alias, `fxg1a`, for compatibility with fxSDK up to 2.7 for which there was no g3a file editor in the fxSDK.

**Asset conversion** with `fxconv`

`fxconv` is a programmable asset converter that converts images, fonts and other common asset types into data structures usable directly in add-ins. The built-in formats include gint images and fonts, [libimg](/Lephenixnoir/libimg) images, and binary blobs.

Projects can extend the support to custom types for maps, dialogs, GUI descriptions, or other application-specific assets. Extensions to `fxconv` are implemented in Python within the project.

`fxconv` is tightly integrated into the build system. Normally you declare assets in a `CMakeLists.txt` file, set their parameters in an `fxconv-metadata.txt` file, and then let the build system do the magic.

TODO: Link to gint tutorials or better explain how to use fxconv.

**USB communication** with `fxlink`

`fxlink` is a USB communication tool that can be used to send files to calculators as well as to communicate with gint's USB driver from an add-in. The tool is still it its early stages but already offers two useful features.

Note: `fxlink` does not work within WSL machines on Windows, see [this bug](https://github.com/Microsoft/WSL/issues/2195).

The first feature is interactive communication with add-ins using libusb. This allows add-ins to send text, screenshots, and video captures of their output to a computer in real-time.

The second feature is sending files to fx-CG and G-III calculators (the ones that behave like USB drives) using UDisks2. `fxlink` can mount the calculators, copy files and unmount them from the command-line without root access.

## Using the fxSDK: Build systems

**Building add-ins with CMake**

The official build system is CMake since fxSDK 2.3. When creating a new project, a default `CMakeLists.txt` is generated. There are few deviations from standard CMake practices; refer to a CMake tutorial for general explanations (there is [an fxSDK-specific one in French on Planète Casio](https://www.planet-casio.com/Fr/forums/topic16647-1-tutoriel-compiler-des-add-ins-avec-cmake-fxsdk.html)). The differences are explained below.

Because we are using a cross-compiler, we can't just call `cmake` to configure the project; extra parameters are needed. The `fxsdk build-*` commands call CMake for you with the correct parameters.

The fxSDK provides [a couple of modules](fxsdk/cmake), including:

* [`FX9860G.cmake`](fxsdk/cmake/FX9860G.cmake) and [`FXCG50.cmake`](fxsdk/cmake/FXCG50.cmake) that are loaded automatically by `fxsdk build-fx` and `fxsdk build-cg` respectively. These are one of the reasons why we don't call `cmake` directly. Anything defined here is available to your `CMakeLists.txt`, which includes a number of variables called `FXSDK_*` to give you information on the target and install.
* [`Fxconv.cmake`](fxsdk/cmake/Fxconv.cmake) which provides functions to use fxconv. `fxconv_declare_assets(... WITH_METADATA)` will mark source files as assets to be converted with fxconv, and `fxconv_declare_converters(...)` declares Python modules containing custom conversion functions.
* [`GenerateG1A.cmake`](fxsdk/cmake/GenerateG1A.cmake) and [`GenerateG3A.cmake`](fxsdk/cmake/GenerateG3A.cmake) wrap `fxgxa` and allow you to generate g1a/g3a files for the add-in. The default `CMakeLists.txt` shows how to use them.
* [`FindSimpleLibrary.cmake`](fxsdk/cmake/FindSimpleLibrary.cmake) and [`GitVersionNumber.cmake`](fxsdk/cmake/GitVersionNumber.cmake) are general utilities for libraries. See the [Lephenixnoir/Template-gint-library](https://gitea.planet-casio.com/Lephenixnoir/Template-gint-library) repository for an example of building a library with the fxSDK.

**Building add-ins with make alone**

The original Makefile used to build add-ins is still available. A Makefile-based project can be created with the `--makefile` option of `fxsdk new`. However that Makefile is rarely tested thus occasionally out-of-date, and in general requires you to maintain it through fxSDK updates. It is only advised to use it if you're experienced with make.

## Manual build instructions

The following instructions are part of the manual install process for the fxSDK. You should install this repository first. If you previously had an fxSDK setup, cross-compiler, etc. then you should probably remove them before installing the new one to avoid interference.

The dependencies for this repository are:

* CMake
* libpng ≥ 1.6
* Python ≥ 3.7 (might work in 3.6)
* The Pillow library for Python 3
* libusb 1.0
* The UDisks2 library (unless disabled)

When configuring, you should set an install prefix that you have write access to. I suggest `$HOME/.local`. Note that the cross-compiler *must* be later installed in the path printed by `fxsdk path sysroot`, which is within said prefix.

* Use `-DCMAKE_INSTALL_PREFIX=...` to change the install folder;
* Use `-DFXLINK_DISABLE_UDISKS2=1` to disable UDisks2 support in `fxlink`, if you don't have UDisks2 or you're using WSL.

```bash
% cmake -B build [OPTIONS...]
% make -C build
% make -C build install
```

You can then proceed to install the cross-compiler. If in doubt about the order in which you need to install repositories, refer to the [GiteaPC README](https://gitea.planet-casio.com/Lephenixnoir/GiteaPC) or check the `giteapc.make` files of each repository, where dependencies are listed.
