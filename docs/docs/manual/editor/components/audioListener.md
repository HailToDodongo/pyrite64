# Audio Listener

Drives the 3D audio listener from its object's position and rotation,
using the same convention as a camera (-Z forward, Y-up).\
All [Audio (3D)](audio3d.md) sounds are spatialized relative to it: distance falloff,
stereo panning, low-pass muffling and Doppler all measure against this object.
Distances are in meters, converted with the scene's `Units per Meter` setting.

Place the component wherever the
mix should be perceived from:

- **First-person / camera-view audio**: add it to the camera's object.
- **Third-person**: add it to a dedicated listener object, then use two
  [Constraint](constraint.md) components on it: one copying the **position** of the
  character, one copying the **rotation** of the view object. Distances then measure
  from the character while left/right matches the screen.
- **Fully scripted**: move and rotate the listener object from a script, or call
  `Audio::engine().setListener()` directly.

Exactly one listener should be enabled at a time,
toggle the object active states to switch listeners if needed.

## Options

This component has no options.

## See also

- {cpp:struct}`P64::Comp::AudioListener`: the runtime component in the C++ API.
