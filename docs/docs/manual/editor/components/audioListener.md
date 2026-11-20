# Audio Listener

Sets the position and rotation all [Audio (3D)](audio3d.md) sounds are heard from.\
By default it follows its own object, using the same directions as a camera (-Z forward, Y up).

Position and rotation can be set independently to handle different case.\
For Example:

- **First-person**: add to the camera object, or keep both options on `<Self>` and set **Rotation** to `Active Camera`.
- **Third-person**: set **Position** to the character and **Rotation** to `Active Camera`.
  This way loudness if relative to the player, while orientation match the current view.
- **Scripted**: move the listener object from a script, or call `AudioManager::setListener()` directly.

The position and rotation are read at the end of the frame, after all objects were updated.

Only one listener should be active at a time.\
Without any active listener, the last position and rotation are kept.

## Options

| Option | Description |
|--------|-------------|
| **Position** | Object the position is taken from, `<Self>` uses the listener's own object. |
| **Rotation** | Either the rotation of the listener's own object, or the direction of the active camera. |

## See also

- {cpp:struct}`P64::Comp::AudioListener`: C++ API
