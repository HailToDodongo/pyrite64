# Custom Graph Nodes

The nodes available in the {doc}`Node Graph <nodeGraph>` editor are not hardcoded.\
Each one is defined by a small JavaScript file that declares what the node looks like,\
the pins and properties it has, and how it generates C++ code.\ 
The builtin nodes are written the same way, and you can add your own.

Custom nodes live in a `nodes/` folder inside your project (`<project>/nodes/*.js`).\
They are loaded when the project opens, and the editor watches the folder:\
saving a file reloads its nodes immediately, with no restart.

## A first node

Create `nodes/logMessage.js` in your project:

```js
node({
  id: "game.logMessage",
  name: icon("message-text-outline") + " Log Value",
  color: [120, 120, 200],
  category: "Test",
  inputs:  [logicIn(), valueIn("x", "f32")],
  outputs: [logicOut()],
  props: {
    text: Str({ label: "Text" }),
  },
  build(n, ctx) {
    ctx.line(`P64::Log::info("${n.text}: %f", ${ctx.inputExpr(0)});`);
  },
});
```

Save it, and "Log Value" appears in the create menu under a "Debug" group.\
When a graph using it is built, the `build` function runs and emits the C++ line into the
generated source.

## Setup of a node

A node is registered by calling `node({ ... })` with these fields:

| Field      | Description                                                                                                        |
|------------|--------------------------------------------------------------------------------------------------------------------|
| `id`       | Stable unique identifier, e.g. `"game.logMessage"`.<br>Saved into graphs, so do not change it once in use.         |
| `name`     | Label shown in the create menu and as the node title. Usually `icon("...") + " Name"`.                             |
| `color`    | Title-bar color as `[r, g, b]` or `[r, g, b, a]` (0 to 255), or a named `Color` (e.g. `Color.orange`).             |
| `category` | Optional group name in the create menu.                                                                            |
| `rounding` | Optional corner rounding of the node box.                                                                          |
| `entry`    | Set `true` for a start node (emitted first, no inputs). Normally omitted.                                          |
| `hidden`   | Set `true` to keep the node out of the create menu (e.g. a deprecated node). It still loads in graphs that use it. |
| `inputs`   | Array of input pins (see [Pins](#pins)).                                                                           |
| `outputs`  | Array of output pins.                                                                                              |
| `props`    | Object of editable properties keyed by name (see [Properties](#properties)).                                       |
| `title`    | Optional dynamic title template (see [Dynamic titles](#dynamic-titles)).                                           |
| `build`    | Function emitting the node's logic-flow code (see [Generating code](#generating-code)).                            |
| `value`    | Function returning the node's value expression, for pure value nodes.                                              |

All IDs of internal nodes start with `core.`,\
to avoid collisions with those or potentially nodes from other people you wish to use,\
it is recommended to prefix your own nodes with the games name.

## Pins

There are two kinds of pin. **Logic** pins (grey) carry control flow: the order in
which nodes run. **Value** pins carry a value that is computed and fed into another
node, and have a **data type** that also sets the pin colour.

```js
inputs:  [logicIn(), valueIn("Amount", "f32")],
outputs: [logicOut("Done"), valueOut("f32")],
```

| Helper | Pin |
|--------|-----|
| `logicIn(name)` | Logic input |
| `valueIn(name, type, default)` | Value input of the given [data type](#value-types) |
| `logicOut(name)` | Logic output |
| `valueOut(type, name)` | Value output of the given [data type](#value-types) |

The `name` is optional and shown as a small label next to the pin.

A **scalar** value input (an int, uint or float) automatically gets an inline editable
field on the node, used while the pin is unconnected. `default` is its starting value
(it also seeds `inputExpr`'s fallback, below). You don't wire this up yourself: just
declare the input and read it with `ctx.inputExpr(i)`.

## Value types

Every value pin declares a data type. The editor only lets you connect an output to an
input when their types are compatible, and the generated code inserts the matching
conversion when they differ but are convertible.

| Type id | Meaning | C++ type |
|---------|---------|----------|
| `u32` | Unsigned integer | `uint32_t` |
| `i32` | Signed integer | `int32_t` |
| `f32` | Float | `float` |
| `vec3` | 3-component vector | `fm_vec3_t` |
| `quat` | Quaternion (rotation) | `fm_quat_t` |
| `objref` | An object reference | `uint16_t` |

The three numeric types (`u32`, `i32`, `f32`) convert freely into one another.\
The vector and object types are isolated: an object reference cannot be wired into an
arithmetic input, even though it is stored as an integer.

The type table lives in `data/nodes/_types.js` and is itself data-driven.\
A new type is one `valueType(id, { name, cType, color, default, size })` line, and
`convert(from, to, tmpl)` declares an allowed conversion (with `{}` standing for the
source expression). `size` is the storage size in bytes when the type is used as a
graph {ref}`variable <nodegraph-variables>` (default 4, and best kept a multiple of 4 for
alignment):

```js
valueType("u8", { name: "Byte", cType: "uint8_t", color: [200, 200, 90], default: "0", size: 4 });
convert("u8", "i32", "(int32_t)({})");
```

If a custom `cType` lives in your own header, have the nodes that emit it pull the header
in with `ctx.include("<user/myType.h>")` during code generation (see the `ctx` table
below).
The generated file then includes it.

## Properties

Properties are values edited directly on the node. Declare them in `props`, keyed by
the name you will read in `build`/`value`:

```js
props: {
  count: U32({ width: 50 }),
  mode:  Enum({ values: ["once", "loop"], label: "Mode" }),
},
```

| Type | Editor widget |
|------|---------------|
| `U16`, `U32`, `Int` | Integer input |
| `Float` | Float input |
| `Bool` | Checkbox |
| `Str` | Text input |
| `Scene` | Scene picker (a scene from the project) |
| `Enum` | Dropdown |

Every property type accepts an options object: `label` (text beside the widget),
`width` (widget width in pixels), `default` (initial value),
`hideIfInputConnected` (a value-input pin index, hiding the widget while that pin is
connected so a fed-in value takes over, e.g. **Load Scene**'s scene picker), and
`compact` (render the field at text-line height so it lines up with its paired input
pin). `Enum` additionally takes `values` (the list of options) and an optional parallel
`icons` array.

For a plain scalar constant you don't need a property at all: a bare `valueIn` already
provides the inline field (see [Pins](#pins)). Use `hideIfInputConnected` only when
the input needs a non-scalar editor, like a scene or asset picker.

## The node view (`n`)

Inside `build` and `value`, the first argument `n` exposes the current property values
by name:

```js
n.count          // the stored number
n.text           // the stored string
```

Enum properties resolve to an object so you can use either the chosen option or its
icon:

```js
n.mode.value     // the selected string, e.g. "loop"
n.mode.index     // the selected index, e.g. 1
n.mode.icon      // the option's glyph (when icons were given)
```

`n.res()` returns the name of the global variable that holds this node's value output
(see [Value nodes](#value-nodes)).

## Generating code

The second argument `ctx` is the code generator. A node's `build` function calls its
methods to append C++ to the graph's generated `run()` function.

| Method | Emits |
|--------|-------|
| `ctx.line(str)` | A raw statement line. |
| `ctx.localConst(type, name, value)` | `constexpr <type> <name> = <value>;` |
| `ctx.localVar(type, name, value)` | `<type> <name> = <value>;` |
| `ctx.globalVar(type, name, value)` | A persistent variable (declared once at the top of the run). |
| `ctx.globalVar(type, value)` | Same, but auto-names it and returns the name. |
| `ctx.declareVar(type, name, value)` | Like `globalVar`, but ignored if a variable of that `name` was already declared (for variables shared by name across nodes). |
| `ctx.setVar(name, value)` | `<name> = <value>;` |
| `ctx.incrVar(name, value)` | `<name> += <value>;` |
| `ctx.jump(outIndex)` | Continue execution at the node wired to output `outIndex`. |
| `ctx.include(path)` | Add an `#include` to the generated file (e.g. for a custom value type's header). `path` is the text after `#include `, so pass the delimiters: `ctx.include("<user/myType.h>")` or `ctx.include('"local.h"')`. Duplicates (including the engine's own includes) are ignored. |
| `ctx.inputExpr(valueIndex, fallback)` | A C++ expression for the value wired into value input `valueIndex`, converted to that input's declared type. When nothing is connected it yields the optional `fallback`, otherwise the input's inline field value (scalar inputs), or a typed zero (e.g. `fm_vec3_t{}`). |
| `ctx.hasValueInput(valueIndex)` | Whether value input `valueIndex` is connected. |
| `ctx.inputType(valueIndex)` | The type id of value input `valueIndex`. |
| `ctx.inputCType(valueIndex)` | The C++ type of value input `valueIndex`. |

A numeric or string passed as a `value` is turned into a C++ token automatically.

The statement-emitting methods return `ctx`, so calls can be chained:

```js
ctx.localConst("int", "a", n.count)
   .localVar("int", "b", "a * 2")
   .line("doThing(a, b);");
```

The value-returning methods (`globalVar(type, value)`, `inputExpr`, `hasValueInput`)
return their value instead, so they end a chain.

Execution flows from a node to whatever its first logic output connects to, so a
simple node with one logic output does not need to call `ctx.jump`. Call `ctx.jump(i)`
only when you branch to a specific output, like an if/else:

```js
build(n, ctx) {
  ctx.localVar("int", "t_cmp", ctx.inputExpr(0));
  ctx.line("if(t_cmp) {");
  ctx.jump(0);          // the "True" output
  ctx.line("} else {");
  ctx.jump(1);          // the "False" output
  ctx.line("}");
},
```

`inputExpr` and `hasValueInput` are indexed over the node's **value** input pins
only (a node's first value input is index `0`, regardless of any logic inputs before
it).

## Value nodes

A node with no logic pins is a pure value node. Instead of `build`, it implements
`value`, which returns a C++ expression string. It is evaluated lazily: the expression
is only produced when another node reads the output, and reading chains of value nodes
resolves them all the way down.

```js
node({
  id: "math.add",
  name: icon("plus") + " Add",
  color: [255, 153, 85],
  inputs:  [valueIn("A", "f32"), valueIn("B", "f32")],
  outputs: [valueOut("f32")],
  value(n, ctx) {
    return `(${ctx.inputExpr(0)} + ${ctx.inputExpr(1)})`;
  },
});
```

A `Value` feeding an `Add` feeding a `Compare` collapses into a single expression like
`(res_a + res_b)` at the point it is used.

When the expression needs temporaries or a function that takes out-parameters (common
with the `fm_vec3_*` / `fm_quat_*` helpers, which write through a pointer), wrap the body
in `lambda(...)`. It produces an immediately-invoked capturing lambda, so the node stays
a single value expression and still inlines under optimization:

```js
value(n, ctx) {
  return lambda(`fm_vec3_t a = ${ctx.inputExpr(0)}, b = ${ctx.inputExpr(1)};
                 fm_vec3_t r; fm_vec3_cross(&r, &a, &b); return r;`);
}
```

If a node needs to compute and store a value once (rather than inline it every time),
write a `build` that assigns `n.res()`, for example
`ctx.globalVar("int32_t", n.res(), 0)`. Declare the global with the C++ type of the
node's value output so back-propagation reads it back at the right type. Consumers then
read it automatically.

### A flow node that also returns a value

`build` and `value` aren't either/or: a node can run as part of the control flow **and**
expose a result. Give it both a logic output and a value output, and in `build` stash the
result in `n.res()` (the node's persistent result variable). Do **not** add a `value`
function: with none, anything reading the value output back-propagates to `n.res()`. This
is the right shape for a side-effecting call whose return you want to use later (a dialog
that returns the chosen option, a spawn that returns an id, ...):

```js
node({
  id: "game.dialog",
  name: icon("message-text-outline") + " Dialog",
  inputs:  [logicIn()],
  outputs: [logicOut(), valueOut("i32")], // continues the flow + yields a result
  props: { msg: Str({ label: "Message" }) },
  build(n, ctx) {
    ctx.include("<user/systems/dialog.h>");
    ctx.globalVar("int32_t", n.res(), 0);                          // declared once, up top
    ctx.setVar(n.res(), `User::Dialog::showMessage("${n.msg}"_hash)`); // assigned in the flow
  },
});
```

Declare the result global with a default and assign it inside `build` (not via
`ctx.globalVar(type, n.res(), expr)`), so the side-effecting call runs when the flow
reaches the node, not once at the top of the graph.

## Helpers

These globals are available in any node file:

| Helper | Purpose |
|--------|---------|
| `icon(name)` | The glyph for a Material Design Icons name, e.g. `icon("clock-outline")`. |
| `crc32(text)` | A 32-bit hash matching the engine, for string ids. |
| `lambda(body)` | Wraps a C++ statement body in an immediately-invoked capturing lambda (`[&]{ ... }()`), so a `value` node can use temporaries while staying a single expression. |
| `numberOrHash(v)` | Returns a numeric literal unchanged, otherwise a hashed string token (`"foo"_hash`). |
| `typeColor(id)` | The `[r, g, b]` color of a value type, e.g. to color a node to match its data type. |
| `Color` | Named node colors: `Color.orange`, `Color.green`, `Color.red`, `Color.grey`, `Color.white`. |
| `typeCType(id)` | The generated C++ type string for a value type id. |

## Dynamic titles

The `title` field is a template whose `{key}` placeholders are filled from the node's
properties, and `{key.icon}` resolves an enum property's current glyph. It updates as
the property changes:

```js
title: "{op.icon} Compare",
```

## Notes

- `build` and `value` run only when a graph is built, never every frame. They produce
  text, and cannot draw the node or access the editor.
- Scripts run in a sandbox with no file system or OS access.
- If a node file has an error, it is reported in the editor log and the affected nodes
  are skipped. The rest keep working.
- A graph referencing a node whose definition is missing keeps the node as a
  placeholder so no data is lost. It reappears intact once the definition is available.

## See also

- {doc}`Node Graph <nodeGraph>`: using the node-graph editor.
- {doc}`Node Graph component <../editor/components/nodeGraph>`: attaching a graph to an object.
