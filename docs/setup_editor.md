# Installing Pyrite64

Pyrite64 comes as a self-contained installation for the editor and engine.<br>
The only thing external is the N64-SDK,<br>
which depending on the platform, can be auto-installed for you by the editor.

Below are instructions to get Pyrite64 running on each supported platform:

## Windows & Linux

(@TODO: make auto release on github)

Download the latest prebuilt version from GitHub:<br>
https://github.com/HailToDodongo/pyrite64/actions <br>
(Alternatively, you can build the editor from [source](./build_editor.md))

Extract the ZIP file on your PC in any directory you want.<br>
Inside, launch `pyrite64.exe` (or `pyrite64` on linux) to start the editor.

On the first start you will see a message that it could not find any N64 toolchain.<br>
By clicking on the "Install Toolchain" button you can open the toolchain manager.<br>
This will walk your through the process of installing it, most of which is fully automated.<br>

Once that part is done, you now have two new options to either open or create a new project.

### Creating a new Project

As a test if everything has installed correctly, you can create a new project.<br>
Simply click on "Create Project" and choose a name.<br>
Once in the editor, press `F12` to build the default project.<br>
This should then succeed and produce a `.z64` ROM in the project directory.<br>

Note that the first build can take a little bit longer, since the included engine gets compiled once.

---
Next: [Using the Editor](./usage_editor.md)
