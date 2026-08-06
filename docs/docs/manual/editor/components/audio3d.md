# Audio (3D)

Plays a sound effect positioned in 3D space at the object's position.\
The engine spatializes it against the scene's [Audio Listener](audioListener.md):
distance based volume falloff, stereo panning, a distance low-pass filter,
distance reverb and optional Doppler.
While the sound plays it automatically follows the object.

Only samples (`.wav` converted to `.tsw`) can be positioned, streamed audio and
sequences cannot. Prefer mono samples: a stereo sample plays both channels and
mostly defeats the positioning.

All distances below are in **meters**, converted from world units with the
scene's `Units per Meter` setting, so they match the physics units rather than
raw object coordinates.

With **Loop** enabled the sound becomes a **persistent emitter**: it plays until
stopped, frees its voices while the listener is out of range, and seamlessly
revives them once back in range (or after voices were stolen under pressure).
This needs the "Loop" setting enabled on the asset too, which bakes the loop point.

## Options

| Option | Description |
|--------|-------------|
| **Audio** | The sample asset to play. |
| **Volume** | Base volume, scaled further by the distance falloff. |
| **Pitch** | Pitch offset in semitones (12 = one octave up). Doppler is applied on top of it. |
| **Loop** | Plays as a persistent looping emitter until stopped (needs the asset's "Loop" setting). |
| **Auto-Play** | When enabled, playback starts automatically when the object spawns. |
| **Near Dist.** | Distance in meters up to which the sound plays at full volume. |
| **Max Dist.** | Distance in meters at which the sound becomes silent. |
| **Rolloff** | Falloff factor between near and max distance, higher values fall off faster. |
| **Doppler** | Doppler effect strength, 0 disables it. |
| **Low-Pass Start** | Fraction of the max distance where the low-pass filter starts closing, 1 disables it. |
| **Reverb Near** | Reverb send at the near distance (needs the scene reverb enabled). |
| **Reverb Far** | Reverb send at the max distance. |

## See also

- {cpp:struct}`P64::Comp::Audio3D`: the runtime component in the C++ API.
