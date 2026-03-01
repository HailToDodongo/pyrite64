3D Models
====================================

3D Models are handled internally by the [tiny3d](https://github.com/HailToDodongo/tiny3d) library. \
The only accepted source format is GLTF, exported as `.glb`. \
This will be used for the mesh itself, materials, skeletons and animations.

## Limitations

### Meshes
The is no limit on the amount of meshes, vertices or triangles. \
However keep in mind the N64 can only handle so much before slowing down.

Vertex attributes are quantized for performance and hardware reasons:
- Position -> 16bit signed integer per component
- Color -> RGBA32 (8 bit per channel)
- UV -> 16bit unsigned integer
- Normals -> 5.6.5 bit normalized

All of this is handled automatically, and usually not noticeable. \
The only thing to keep in mind is the vertex position part. \
Pyrite64 offers a pre-scaling factor during model conversion, defaulting to x32. \
What this means is if e.g. a vertex has the position 1.25, it will be stored as 40 in the final data. 

You can compensate for this by scaling the model up or down in the editor. \
Please be aware that wanting 1:1 units with blender will cause precision issues eventually. \
So try to not rely on it too much. \
If you notice models looking very blocky, you can try increasing the pre-scaling factor.

By default, the editor tries to show a good scaling range, 
so try to keep the runtime (not model) scales something along the lines of 1unit = 1cm.

### Materials
All materials need to created with [fast64](https://github.com/Fast-64/fast64). \
This is required since the RDP has a fixed function pipeline, \
and there is no large overlap to a standard PBR material.

Embedded textures are not supported, \
textures must be present as PNG files in the assets folder.

### Skinning
While there is no limit on bones or animations, bone-weights are not supported. \
This means that each vertex can only be influenced by a single bone at full force, \
and by extension a triangle can be influenced by at most 3 bones.

In case weight-painting is used, the converter will pick the bone with the highest weight.

Please also make sure that for skinned meshes, \
neither the model nor armature itself has any transforms. \
You can apply those in blender to avoid potential issues.

### Animation
Animations are only supported for bones. \
You cannot use it to transform objects themselves, or drive other values (e.g. materials).

Position, rotation and non-uniform scaling is supported.

## Creating Models

### Setup

Since fast64 is required, models need to be authored from blender. \
You can find both projects here:

- [Fast64](https://github.com/Fast-64/fast64)
- [Blender](https://www.blender.org/)
    
For blender, use at least version `4.0`, but not `5.0` or newer. \
Install fast64 as a plugin, you can do this by downloading the repo as a zip:

```{image} /_static/img/fast64_download.png
:alt: Fast64 Download
:width: 400px
:align: center
```

In blender under `Preferences -> Add-ons -> Install from disk`, \
you can then select downloaded zip file.

WIP

### Modeling

WIP

### Export

WIP

## Performance
