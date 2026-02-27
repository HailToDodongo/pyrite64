Building the Editor
====================================

| Below are instructions to build the editor on either Linux or Windows.
| Note that due to a small dependency on libdragon, GCC is required for now.
| On Windows, that means building via MSYS2.

====================
Prerequisites
====================

Before building the project, make sure you have the following tools installed:

* CMake
* Ninja
* GCC with at least C++23 support
* Git LFS

Linux users should follow the conventions of their distribution and package manager for all packages.

| Windows users need to make sure a recent version of MSYS2_ is installed.
| Open an MSYS2 terminal in the ``UCRT64`` environment, and install the UCRT-specific packages for the dependencies:

.. code-block:: sh

  pacman -S mingw-w64-ucrt-x86_64-gcc
  pacman -S mingw-w64-ucrt-x86_64-cmake
  pacman -S mingw-w64-ucrt-x86_64-ninja


====================
Git LFS
====================

On some Linux distributions, Git LFS may require adding an external repository to your package manager per these 
`instructions <https://github.com/git-lfs/git-lfs/blob/main/INSTALLING.md>`__.

Windows users should already have Git LFS installed as part of Git for Windows. You can verify this by running:

.. code-block:: sh

  git lfs version

If no version is shown, install Git LFS from their website (https://git-lfs.com/).

After installing Git LFS, initialize it by running:

.. code-block:: sh

  git lfs install

If you already cloned the ``pyrite64`` repository before initalizing Git LFS, navigate to the repository root folder and run:

.. code-block:: sh

  git lfs update


====================
Build Instructions
====================

After cloning the ``pyrite64`` repository, make sure to fetch all the submodules:

.. code-block:: sh

  git submodule update --init --recursive


To configure the project, run:

.. code-block:: sh

  cmake --preset <preset>

After that, and for every subsequent build, run:

.. code-block:: sh

  cmake --build --preset <preset>

Where ``<preset>`` is replaced with the CMake preset name corresponding to your system:

* ``linux`` for Linux systems
* ``windows-gcc`` for Windows systems with MSYS2

| Once the build is finished, a program called ``pyrite64`` (or ``pyrite64.exe``) should be placed in the root directory of the repo.
| The program itself can be placed anywhere on the system, however the ``./data`` and ``./n64`` directories must stay next to it.

To open the editor, simply execute ``./pyrite64`` (or ``.\pyrite64.exe``).


.. _MSYS2: https://www.msys2.org/