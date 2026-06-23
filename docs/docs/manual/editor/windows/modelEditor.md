# Model Editor

The **Model Editor** opens a single 3D model asset for inspection and material editing. Open it
by opening a model from the {doc}`Files <assetBrowser>` browser, or with **Open Model Editor**
on a {doc}`Model component <../components/model>`. Each model opens in its own window.

## Materials

The bulk of the editor is the model's material list. Each material can be expanded to edit how
that part of the model is drawn:

- **Override**: by default a material uses the project's defaults; enable this to customise it.
- **Color combiner**: the 1- or 2-cycle combiner inputs that define how colors and textures are
  mixed.
- **Textures**: the texture(s) used, including size and placeholder settings.
- **Render flags**: options such as Unlit, Vertex FX, Fog to Alpha, and front/back face culling.
- **Blending and fog**: the blend mode and fog behaviour for the material.

These settings are the per-model side of the material system. For the full explanation of
combiners, textures and how materials and material instances relate, see the material docs.

## See also

- {doc}`Materials overview <../materials/overview>`: how the material system works.
- {doc}`Material <../materials/material>` and {doc}`Material Instance <../materials/instance>`.
- {doc}`Model component <../components/model>`: using a model on an object.
- {doc}`3D Models <../../assets/model3d>`: the model import pipeline.
