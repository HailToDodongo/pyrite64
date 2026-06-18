# Audio (2D)

```{image} /_static/img/ui_comp_audio_2d.png
:align: center
```

Plays a non-positional (2D) sound or music track tied to the object.\
"2D" means the playback is not spatialized: it does not change with the object's position relative
to the listener.

## Options

| Option | Description |
|--------|-------------|
| **Audio** | The audio asset to play. |
| **Volume** | Playback volume. |
| **Loop** | When enabled, the audio repeats when it reaches the end. |
| **Auto-Play** | When enabled, playback starts automatically when the object spawns. |

## See also

- {cpp:struct}`P64::Comp::Audio2D`: the runtime component in the C++ API.
