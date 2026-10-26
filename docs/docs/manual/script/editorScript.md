# Editor Scripts

Editor scripts are JavaScript files that run inside the editor, not in the game.\
They can automate tasks in the editor, although for now the exposed functions are limited, and the entire feature is still very much WIP.\
JavaScript wise, modern ES6 syntax is largely supported.

## Creating Scripts

Place `.js` files in the `scripts/` folder of your project.\
They show up in the "Scripts" tab of the asset browser.

Selecting a script shows its code in the asset preview.\
From there you can edit it, save it with **Save**, and execute it with **Run**.\
The file can also be edited in an external editor at the same time, the preview reloads it when it changes on disk.

## Console

Below the log in the **Log** window is an input field to run single lines of JavaScript.\
Press Enter to execute a line, the result is printed to the log.\
Use the up and down keys to go through previously entered lines.

All scripts and the console share the same context while a project is open.\
Variables or functions defined in one are available in the others, until the project is closed.

## Functions

### editor

| Function | Description |
|----------|-------------|
| `editor.build(run)` | Builds the project, also runs it if `run` is `true`.<br> Returns `false` if a build is already in progress |
| `editor.save()` | Saves the project and the open scene |
| `editor.clean()` | Cleans the project's build files |
| `editor.close()` | Closes the project |
| `editor.reloadAssets()` | Reloads all assets |
| `editor.addAction(id, name, callback)` | Adds an {doc}`action <../editor/actions>` that can be run from the action search (F3).<br> Calling it again with the same id replaces the callback, ids of built-in actions can't be used |

Example of a custom action:

```js
editor.addAction("test.hello", "Say Hello", () => {
  console.log("Hello!");
});
```

### console

| Function | Description |
|----------|-------------|
| `console.log(value)` | Prints a value to the log |
| `console.clear()` | Clears the log |

### scene

| Function | Description |
|----------|-------------|
| `scene.load(id)` | Opens the scene with the given id. Returns `false` if the id is invalid |
