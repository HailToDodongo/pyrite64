# Node Graph

```{image} /_static/img/ui_comp_node_graph.png
:align: center
```

Attaches a visual node-graph script to the object, an alternative to a
{doc}`Code <code>` component for behavior authored as a graph instead of C++.

## Options

| Option | Description |
|--------|-------------|
| **File** | The node-graph asset to run. Use **Edit** to open it in the graph editor, or **Create** to make a new graph (you'll be prompted for a name). |
| **Auto Run** | When enabled, the graph starts automatically when the object spawns. |
| **Repeatable** | When enabled, the graph may be run more than once. |
| **Object references** | If the graph declares "Object" nodes, each one appears here as a slot where you pick the scene object it should refer to. The slots update whenever you change the selected graph. |

## See also

- {cpp:struct}`P64::Comp::NodeGraph`: the runtime component in the C++ API.
