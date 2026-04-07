# Material Instance

Now for the instance in each individual object / component.\
If you open the settings for a `Model (Static)` or `Model (Animated)` component,\
you will find a section for the material instance.\
All values you are allowed to set (so not already set by the material) will show up here.\
Note that this is for the entire model, spanning across all materials it may draw.

## Basic Settings

```{image} /_static/img/mat_inst_default.png
:align: center
:width: 400px
```

By default, you will see the following options:

`Depth`, allows overriding depth for the entire model.\
This only makes sense if the model itself never overrides it.\
Please also read the docs for `Depth` in the materials setting.

`Prim-Color` / `Env-Color` lets you set generic color registers.

`Fresnel` (this setting is still WIP and therefore not document yet)

## Placeholder Slots

If you set any texture to be a placeholder, you will see additional settings.\
As mentioned, settings span all materials, so all placeholders are listed at once here.\
In the instance they are exposed as numbered "Slots".\
The editors viewport will correctly preview this, so any changes made are reflected in real time.

```{image} /_static/img/mat_inst_full.png
:align: center
:width: 400px
```

If the placeholder is of the type `Tile`,\
you will see an `Offset` option.
This allows you to scroll the texture on each axis.

If you set the placeholder to `Texture + Tile`,\
you will see the full texture settings like you would in the material.\
Allowing you fully set any texture or tile settings you want specifically for this object.

Please be aware that UVs at runtime are in pixel coordinates.\
Usually this doesn't matter as the model converter converts it based on the texture used.\
For placeholders, this can no longer be done and the `Size` setting in the material is used.\
If you plan on using differently sized textures in the placeholder later on, UVs may no longer match.\
You can compensate for this with the tiles `Scale` setting.\
Once again, the editor will preview this correctly, so you can see any potential issues.

## C++ API 

In many cases you want to dynamically change settings at runtime,\
for that the C++ API can be used.\
As mentioned before, materials themselves are immutable at runtime,\
so changing the material instance is the only supported way to do so.

In general, everything you can set in the editor can also be set via C++.\
As long as the material was setup to allow for dynamic values in the first place.

### Accessing the instance

Inside an object-script, first grab the mesh component:

```c++
auto model = obj.getComponent<Comp::Model>();
// or for animatied models:
auto model = obj.getComponent<Comp::AnimModel>();
```
In case you have multiple ones, you can also pass an index as the first argument.\
If a given component doesn't exist, you will get a `nullptr` back.\
Unless you are always guaranteed to have one, please check for that case.

Then access the material instance, this member always exists:
```c++
auto &mat = model->getMatInstance();
 ```
 
### Setting values
For values, you can directly change the members of the struct, for example:
```c++
mat.colorEnv = {0xFF, 0xFF, 0xFF, 0xFF};
mat.colorPrim.a = 42;
```
Currently, both `Prim-Color` and `Env-Color` are exposed.\
If the material doesn't use or allow for overides, you can still safely change the values,\
in which case they will simply have no effect.\
If they are used, they will get picked up the next time the model is drawn. 

### Setting Placeholders
To access placeholders, use the getter for it:
```c++
auto ph = mat.getPlaceholder(0);
```
The first argument is the index, and corresponds to the slot-number you see in the editor UI.\
If a placeholder doesn't exist, a `nullptr` is returned.

The placeholder currently has two members accessible:
```c++
struct Placeholder
{
  Material::Tile tile{};
  void update();
};
``` 
`tile` refers to the tile-settings and texture asset you can see in the UI.\
This object can be modified in order to do effects like tile-scrolling or dynmaic textures.\
The `update()` method must then be called after making changes to apply them.\
Note that you are not allowed to call `update()` multiple times per frame, please only do so once.

As an example, to change the texture currently used:
```c++
ph->tile.setTexture("face01.i4.sprite"_asset);
ph->update();
```
Or to set the scroll of texture to some value:
```c++
ph->tile.setOffset(0.0f, data->scroll);
ph->update();
```

Besides the helper functions, you are also allowed to modify the `s`/`t` members directly.\
The general structure looks like this:
```c++
struct Tile
{
  TileAxis s;
  TileAxis t;
  void setTexture(uint16_t assetIdx);
  void setOffset(float offsetS, float offsetT);
};

struct TileAxis {
  uint16_t offset;
  uint16_t repeat;
  int8_t scale;
  int8_t mirror;

  void setOffset(float offs);
  void setRepeat(float rep);
};
```
However, ideally always go through the setter, as values internally are stored in fixed-point rather than floats.\
The use-case to do so directly is to avoid redundant states, or to micro-optimize performance.

You can safely modify tile or texture settings even if the placeholder doesn't expose it.\
In that case, the changes simply won't have any effect.

#### Example

Here is a full example of an object-script scrolling a texture:
```c++
#include "script/userScript.h"
#include "scene/sceneManager.h"
#include "scene/components/model.h"

namespace P64::Script::C751DA2F182978DE
{
  P64_DATA(
    float scroll;
  );

  void init(Object& obj, Data *data)
  {
    data->scroll = rand() % 128;
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    data->scroll = fmodf(data->scroll + deltaTime*32.0f, 256.0f);

    auto &mat = obj.getComponent<Comp::Model>()->getMatInstance();
    auto ph = mat.getPlaceholder(0);
    if(!ph)return;

    ph->tile.setOffset(0, 256 - data->scroll);
    ph->update();
  }
}
```

You can find further examples in the example project `material_test`.\
Or for dynamic textures, in the `jam25` exmaple in `Player.c