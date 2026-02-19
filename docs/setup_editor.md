# Installing Pyrite64

Pyrite64 comes as a self-contained installation for the editor and engine.<br>
The only thing external is the N64-SDK,<br>
which depending on the platform, can be auto-installed for you by the editor.

Below are instructions to get Pyrite64 running on each supported platform:

## Windows & Linux

Download the latest release from:
https://github.com/HailToDodongo/pyrite64/releases
<br>Or for the latest development build from the GitHub Actions:
https://github.com/HailToDodongo/pyrite64/actions
<br>(Alternatively, you can build the editor from [source](./build_editor.md))

Extract the ZIP file on your PC in any directory you want.<br>
Inside, launch `pyrite64.exe` (or `pyrite64` on linux) to start the editor.

On the first start you will see a message that it could not find any N64 toolchain.<br>
By clicking on the "Install Toolchain" button you can open the toolchain manager.<br>
This will walk your through the process of installing it, most of which is fully automated.<br>

Once that part is done, you now have two new options to either open or create a new project.

<img src="./img/editor01.png" width="450">

### Creating a new Project

As a test if everything has installed correctly, you can create a new project.<br>
Simply click on "Create Project" and choose a name.<br>
Once in the editor, press `F12` to build the default project.<br>
This should then succeed and produce a `.z64` ROM in the project directory.<br>

Note that the first build can take a little bit longer, since the included engine gets compiled once.

### Opening a Project
Projects are contained to a directory, which includes a `.p64proj` file that holds the project settings.<br>
To open an existing project, click on "Open Project" and select the `.p64proj` file.<br>
Since it has a unique file extension, you can also associate it with the editor in your OS to open it by default when double-clicking on it.<br>

Bundled with the editor is also an example project called "Cathode Quest 64" (N64Brew 2025 GameJam entry).<br>
This is located in `n64/examples/jam25`, and is going to be updated over time as the editor grows.<br>

<img src="./img/editor00.png" width="450">

---
Next: [Using the Editor](./usage_editor.md)
