# Collision-Mesh

```{image} /_static/img/ui_comp_coll_mesh.png
:align: center
```

A static triangle-mesh collider built from a 3D model.\
This is typically used for level geometry that other objects collide against.\
It can either be something static (e.g. map mesh), or transforming objects like moving platforms.

Note that collision between collision-meshes is not handled due to the cost and complexity.

## Options

| Option | Description |
|--------|-------------|
| **Reacts to** | The collision layers this mesh reads (which layers it collides with). Select one or more named layers. |
| **Is Affecting** | The collision layers this mesh writes (which layers see it). |
| **Model** | The 3D model asset whose triangles are used as the collision mesh. |
| **Mesh Filter** | Expression to include or exclude individual sub-meshes of the model from the collider. Matching meshes are listed below the field. |

## See also

- {doc}`Collision & Physics <../collision>`: general collision & physics docs.
- {cpp:struct}`P64::Comp::CollMesh`: the runtime component in the C++ API.
