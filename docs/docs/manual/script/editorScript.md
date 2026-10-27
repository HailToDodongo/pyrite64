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

Enabling **Auto Run** in the script's settings runs it every time the project is opened.\
Note that this happens before the last opened scene is loaded.

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
| `editor.getSelObjects()` | Returns the selected [objects](#objects) as an array, the primary selection is the last one |
| `editor.addAction(id, name, callback)` | Adds an {doc}`action <../editor/actions>` that can be run from the action search (F3).<br> Calling it again with the same id replaces the callback, ids of built-in actions can't be used |

Example of a custom action:

```js
editor.addAction("test.hello", "Say Hello", () => {
  console.log("Hello!");
});
```

### history

Changes made by scripts are not recorded for undo automatically.\
Call `history.markChanged()` once after a set of changes, so they can be undone as a single step.

| Function | Description |
|----------|-------------|
| `history.markChanged(description)` | Records the current changes as one undo step, `description` is shown in the undo menu |
| `history.undo()` | Undoes the last step. Returns `false` if there is nothing to undo |
| `history.redo()` | Redoes the last undone step. Returns `false` if there is nothing to redo |

### console

| Function | Description |
|----------|-------------|
| `console.log(value)` | Prints a value to the log |
| `console.clear()` | Clears the log |

### scene

| Function | Description |
|----------|-------------|
| `scene.load(id)` | Opens the scene with the given id. Returns `false` if the id is invalid |

## Objects

Functions like `editor.getSelObjects()` return objects of the open scene.\
They stay valid even after undo or redo, but throw an error once the object is deleted.

| Property | Description |
|----------|-------------|
| `uuid` | Unique id of the object (read-only) |
| `name` | Name of the object |
| `pos` | Position as a [vector](#vectors) |
| `rot` | Rotation as a [quaternion](#quaternions) |
| `scale` | Scale as a [vector](#vectors) |

Changing the position, rotation or scale of a prefab instance automatically creates an override for it, same as in the inspector.

## Vectors

Vectors are objects with `x`, `y` and `z`.\
A vector returned from a property like `pos` stays linked to it, so changing a single component changes the property directly.

```js
const obj = editor.getSelObjects()[0];
obj.pos.y += 1;                    // changes a single component
obj.pos = {x: 0, y: 2, z: 0};      // sets all components at once
other.pos = obj.pos;               // copies the position
history.markChanged("Move Objects");
```

When setting a vector, any object with number values for `x`, `y` and `z` is accepted.

## Quaternions

Rotations are quaternions, objects with `x`, `y`, `z` and `w`.

```js
obj.rot = {x: 0, y: 0, z: 0, w: 1};  // no rotation
other.rot = obj.rot;                 // copies the rotation
```

Note that the components are not normalized automatically, call `normalize()` after changing single components.\
It normalizes the quaternion in place, and returns it again.

## Examples

Adds an action that places all selected objects on their average height:
```js
editor.addAction("user.avgHeight", "Average Height", () => 
{
  const objs = editor.getSelObjects();
  if(objs.length == 0)return;

  let h = 0;
  for(let o of objs)h += o.pos.y;
  h /= objs.length;
   
  for(let o of objs) {
    o.pos.y = h;
  }

  history.markChanged("Average Height");
});
```
