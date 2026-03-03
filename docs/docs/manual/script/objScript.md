# Object-Scripts

Now we will focus on how to create a script for an object.

## Creating Scripts

Creating a script is very simple, in the "File" tab under "Scripts" you can see all existing scripts.\
If you click the "+" icon, you can add a new one after entering a file name. 

```{image} /_static/img/script_create.png
:align: center
```
\
Once done, the editor creates it in the `src/user/` directory of your project,\
and now lists it in the editor.

Pyrite64 has no builtin code editor, so an external one (e.g. VSCode, CLion) is required.\
Either navigate to the directory, or right-click on the entry in the editor to open it:

```{image} /_static/img/script_edit.png
:align: center
```
\
Since scripts are internally identified by a UUID stored in the file itself,\
you are free to move or rename them at any point.

## Using Scripts

Select the object you want to attach the script to,\
and add a new "Code" component to it:

```{image} /_static/img/script_add_comp.png
:align: center
```
\
Once done, you can now choose your script in the select-box:\
Alternatively, drag and drop the script from the list view into the select-box.


```{image} /_static/img/script_sel_comp.png
:align: center
```
\
You can add as many different scripts to an object as you want.\
This is useful to compose behaviors by having simple scripts that can be combined.

Using the same script on multiple objects of course also works.\
This happens naturally when you for example place multiple instances of the same object. 

## Script Contents

Now that we have a script created and attached, let's look inside it.

WIP
