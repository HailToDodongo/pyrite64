# Building the Editor

Below are instructions to build the editor on either linux or windows.<br/>
Note that due to a small dependency on libdragon, GCC is required for now.<br/>
On windows that means build through mingw.

## Linux

Before building the project, make sure you have the following tools installed:
- CMake
- Ninja
- GCC with at least C++23 support

After checking the repo out, make sure to also fetch all the submodules:

`git submodule update --init --recursive`

Then to build the project, first create a build directory for it,<br>
then go into that directory and setup CMake:

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
```

After than, and for every subsequent build, run:
```sh
cmake --build . --parallel 8
```

Once finished, a program called `pyrite64` should be placed in the root directory of the repo.
The program itself can be placed anywhere on the system, however the `./data` and `./n64` directory must stay next to it.

To open the editor simply execute `./pyrite64`.

## Windows

Make sure you have a recent version of msys2 installed (https://www.msys2.org/).<br>
Open a msys2 terminal in the `UCRT64` environment, and install the ucrt specific packages for:
- `pacman -S mingw-w64-ucrt-x86_64-gcc`
- `pacman -S mingw-w64-ucrt-x86_64-cmake`
- `pacman -S mingw-w64-ucrt-x86_64-ninja`

Then navigate to the checked out project.<br>
After that, you can follow the linux instructions described above.