# Node Editor

The **Node Editor** is the visual scripting editor for node-graph assets. Each open graph gets
its own window, opened by double-clicking a node-graph asset in the {doc}`Files <assetBrowser>`
browser or from a {doc}`Node Graph component <../components/nodeGraph>` on an object.

## Working in a graph

- **Right-click** the canvas to open the node search and add a node.
- **Drag** from one node's output pin to another node's input to connect them.
- **Drag** nodes to arrange them; pan and zoom the canvas to navigate.
- The graph is saved with the project; unsaved changes prompt before closing.

## Learn more

Node graphs have their own dedicated documentation that covers the concepts, the built-in nodes
and how to author your own:

- {doc}`Node Graphs <../../script/nodeGraph>`: the scripting model and built-in nodes.
- {doc}`Custom Nodes <../../script/nodeGraphCustom>`: writing your own nodes.
- {doc}`Node Graph component <../components/nodeGraph>`: attaching a graph to an object.
