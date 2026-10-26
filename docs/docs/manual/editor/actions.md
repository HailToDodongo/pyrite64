# Actions

Actions are named commands of the editor, like building the project, reloading assets or copying objects.\

Each action has a unique id (e.g. `p64.project.build`) and a display name (e.g. "Build Project").\
Some of them may also only available in certain situations.

## Searching Actions

Press **F3** to open the action search, the shortcut can be changed in the preferences under "Search Actions".\
It lists all actions that are currently available, type to filter them by name or id.

| Key | Description |
|-----|-------------|
| **Up** / **Down** | Changes the selected action |
| **Enter** | Runs the selected action |
| **Escape** | Closes the search |

Clicking an entry runs it too.

## Custom Actions

Editor scripts can add their own actions, which then also show up in the search.\
See `editor.addAction` in {doc}`Editor Scripts <../script/editorScript>`.
