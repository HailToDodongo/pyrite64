# Building the Editor

Below are instructions to build the editor on either linux or windows.<br/>
Note that due to a small dependency on libdragon, GCC is required for now.<br/>
On Windows, that means building via MSYS2.

## Prerequisites

Before building the project, make sure you have the following tools installed:
- CMake
- Ninja
- GCC with at least C++23 support

Linux users should follow the conventions of their distribution and package manager.</br>

Windows users need to make sure a recent version of MSYS2 is installed (https://www.msys2.org/).<br>
Open an MSYS2 terminal in the `UCRT64` environment, and install the UCRT-specific packages for the dependencies:
```sh
pacman -S mingw-w64-ucrt-x86_64-gcc
pacman -S mingw-w64-ucrt-x86_64-cmake
pacman -S mingw-w64-ucrt-x86_64-ninja
```

## Build Instructions

After cloning the `pyrite64` repo, make sure to fetch all the submodules:
```sh
git submodule update --init --recursive
```

To configure the project, run:
```sh
cmake --preset <preset>
```

After that, and for every subsequent build, run:
```sh
cmake --build --preset <preset>
```

Where `<preset>` is replaced with the CMake preset name corresponding to your system:
- `linux` for Linux systems
- `windows-gcc` for Windows systems with MSYS2

Once the build is finished, a program called `pyrite64` (or `pyrite64.exe`) should be placed in the root directory of the repo.<br>
The program itself can be placed anywhere on the system, however the `./data` and `./n64` directories must stay next to it.

To open the editor, simply execute `./pyrite64` (or `.\pyrite64.exe`).
