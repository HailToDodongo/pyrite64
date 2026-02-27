Launcher
============

| When opening Pyrite64, you will be greeted with the launcher.
| If this is your first time opening the editor, it may only show the "Toolchain" option.

.. image:: /_static/img/editor01.png
	:width: 500


Creating a new Project
~~~~~~~~~~~~~~~~~~~~~

| This lets you create a new project, 
| which is a directory of a self-contained game the editor lets you work on.

| After doing so, it is a highly recommended to create a git repository as well.
| By default, a ``.gitignore`` is included too and all visible files can be checked in.

.. image:: /_static/img/launcher_create_project.png
	:width: 500

.. note::
  Due to technical issues with MSYS2/Makefiles, spaces in project paths are not allowed.
 
Opening a Project
~~~~~~~~~~~~~~~~

| Each projects directory includes a ``.p64proj`` file that contains the project settings.
| To open an existing project, click on "Open Project" and select the ``.p64proj`` file.
| Since it has a unique file extension, you can also associate it with the editor in your OS to open it by default when double-clicking on it.

| Bundled with the editor is also an example project called "Cathode Quest 64" (N64Brew 2025 GameJam entry).
| This is located in ``n64/examples/jam25``, and is going to be updated over time as the editor grows.

.. image:: /_static/img/editor00.png
	:width: 500

Toolchain Manager
~~~~~~~~~~~~~~~~~~~~~

| This lets you manage the Libdragon_ and tiny3d_ toolchains, 
| which are required to build and run games.
| Due to a rather large filesize and the need to update them independently, 
| they are not bundled with Pyrite64.

| Instead you can use the toolchain manager to validate an install.
| Or depending on OS, even have it automatically install them for you.

On windows, you have the option to either install or update the toolchain with a single click:

.. image:: /_static/img/launcher_toolchain_install.jpg
	:width: 500

On linux or MacOS, it instead only validates and links relevant documentation to follow:

.. image:: /_static/img/launcher_toolchain_install_linux.png
	:width: 500



.. _tiny3d: https://github.com/HailToDodongo/tiny3d
.. _Libdragon: https://github.com/DragonMinded/libdragon
