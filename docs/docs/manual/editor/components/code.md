# Code

```{image} /_static/img/ui_comp_code.png
:align: center
```

Attaches a C++ script to the object, giving it custom behavior.\
An object can have several Code components to compose behavior from multiple small scripts.\
For how scripts are written, see the {doc}`Object-Scripts <../../script>` guide.

## Options

| Option | Description |
|--------|-------------|
| **Script** | The script asset to attach. You can also drag a script from the file list onto this field. |
| **Arguments** | Any values the script exposes to the editor show up here, with an editor for each (number field, select-box, asset/object picker). A field is exposed by adding a `[[P64::Name("...")]]` attribute to a member of the script's `P64_DATA` block (see {doc}`Object-Scripts <../../script>`, including the `[[P64::Bitmask(...)]]` attribute for named bit flags). |

## See also

- {doc}`Object-Scripts <../../script>`: writing and exposing values from scripts.
- {cpp:struct}`P64::Comp::Code`: the runtime component in the C++ API.
