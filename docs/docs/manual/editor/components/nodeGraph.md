# Node Graph

```{image} /_static/img/ui_comp_node_graph.png
:align: center
```

Attaches a visual node-graph script to the object, an alternative to a
{doc}`Code <code>` component for behavior authored as a graph instead of C++.

For the full docs on how to create and use graphs, check out the {doc}`Node Graph Editor <../../script/nodeGraph>`.

## Options

| Option | Description |
|--------|-------------|
| **File** | The node-graph asset to run. Use **Edit** to open it in the graph editor, or **Create** to make a new graph (you'll be prompted for a name). |
| **Auto Run** | When enabled, the graph starts automatically when the object spawns. |
| **Variables** | Each variable the graph declares appears here as an editor for its per-object starting value (an object picker for Object-typed variables, number or vector fields for the rest). The list updates whenever you change the selected graph. |

## See also

- {doc}`Node Graph <../../script/nodeGraph>`: using the node-graph editor.
- {doc}`Custom Graph Nodes <../../script/nodeGraphCustom>`: defining your own nodes.
- {cpp:struct}`P64::Comp::NodeGraph`: the runtime component in the C++ API.
