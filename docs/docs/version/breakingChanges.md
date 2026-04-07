# Breaking Changes

Breaking Changes by version they were introduced in.

## v0.7.0

This version completely reworked the material system.<br>
tiny3d materials are no longer used, and a strict separation between the material in the model and the instance in the object was done.<br>
Existing projects should still look and run exactly the same, so for settings in fast64 or the editor no changes are needed.<br>
The C++ API did however introduce some breaking-changes.

### Material Instance
Overriding material properties is now done through a "material instance" each mesh component has:

```cpp
// Before:
model->material.colorPrim = {0xFF, 0xFF, 0xFF, 0xFF};

// Now:
model->getMatInstance().colorPrim = {0xFF, 0xFF, 0xFF, 0xFF};
```
This instance now also has additional members for e.g. tile scrolling and dynamic textures.<br>
Any attributes not declared as settable in the editor are ignored even if set on the C++ side. 

### Tiny3D API

Due to no longer using tiny3d materials, the builtin functions that may do so no longer work.<br>
To avoid accidental use, they will now throw a runtime error is used.<br>
This includes the following functions:
```
t3d_model_get_material 
t3d_model_draw_material
t3d_model_draw
t3d_model_draw_custom
t3d_model_draw_skinned
```
There is currently no (public) API replacement,<br>
instead the newly added material options should be used.

Those allow setting additional properties not settable in fast64, as well as
handling things like tile scrolling or dynamic textures.

If you did use those functions before and cannot replace them with the new system,
please open an issue on GitHub so that your use-case can be added officially.


## v0.5.0

The object script `initDelete` function got split into `init` and `destroy`.\
Newly created scripts use the newer version, old scripts will fail on building the project.

To migrate existing scripts, split the existing function. For example:
```cpp
void initDelete(Object& obj, Data *data, bool isDelete)
{
  if(isDelete) {
    rspq_call_deferred((void(*)(void*))rspq_block_free, data->dplBg);
    return;
  }

  rspq_block_begin();
  ...
  data->dplBg = rspq_block_end();
}
```
Becomes:
```cpp
void init(Object& obj, Data *data)
{
  rspq_block_begin();
  ...
  data->dplBg = rspq_block_end();
}

void destroy(Object& obj, Data *data)
{
  rspq_call_deferred((void(*)(void*))rspq_block_free, data->dplBg);
}
```
