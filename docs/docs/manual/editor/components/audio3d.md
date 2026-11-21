# Audio (3D)

Plays a sound at the position of the object.\
Volume and panning depend on where the sound is relative to the [Audio Listener](audioListener.md).

All distances are in meters.\
Note that 3D audio can only work with mono audio files.

## Looping

A looping sound frees its audio channel while the listener is out of range (further than **Max Dist.**).\
Once back in range, it continues where it would be by now, so it sounds as if it was playing the whole time.\
Sounds without loop are stopped once out of range, and are not played at all if already out of range when started.

## Options

| Option | Description |
|--------|-------------|
| **Audio** | The audio asset to play. |
| **Volume** | Base volume, further lowered by the distance. |
| **Loop** | When enabled, the audio repeats when it reaches the end. |
| **Auto-Play** | When enabled, playback starts automatically when the object spawns. |
| **Near Dist.** | Up to this distance the sound plays at full volume. |
| **Max Dist.** | At this distance the sound becomes silent. |
| **Rolloff** | How fast the volume falls off between near and max distance, higher values fall off faster. |

## Scripting

Sounds can also be played in 3D from code, with the same settings:

```cpp
Audio::Conf3D conf{.nearDist = 2.0f, .maxDist = 30.0f};
auto sfx = AudioManager::play3D("sfx/Engine.wav64"_asset, obj.pos, conf, true);

// later in update:
sfx.setPosition(obj.pos);
```

## See also

- {cpp:struct}`P64::Comp::Audio3D`: the runtime component in the C++ API.
- {cpp:func}`P64::AudioManager::play3D`: playing 3D audio from code.
