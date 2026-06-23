# Asset

The **Asset** inspector shows the import settings of the asset currently selected in the
{doc}`Files <assetBrowser>` browser. Different asset types expose different options; some types
(scripts, prefabs) have no import settings and only show their file name.

Changing a setting here re-imports the asset, so it controls how the source file is converted
into the data the game uses.

## Common types

### Images

| Option | Description |
|--------|-------------|
| **Format** | The texture format the image is converted to. The format affects size and quality on the N64. |

See {doc}`Images <../../assets/images>` for details on the texture formats.

### 3D Models

| Option | Description |
|--------|-------------|
| **Base-Scale** | A scale applied to the model on import. Changing it reloads the asset. |
| **Create BVH** | Builds a bounding-volume hierarchy, required for per-object culling on the {doc}`Model <../components/model>` component. |

See {doc}`3D Models <../../assets/model3d>` for the model pipeline.

## See also

- {doc}`Files <assetBrowser>`: select the asset shown here.
- {doc}`Assets <../../assets>`: the asset pipeline and supported types.
