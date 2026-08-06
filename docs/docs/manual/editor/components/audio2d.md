# Audio (2D)

```{image} /_static/img/ui_comp_audio_2d.png
:align: center
```

Plays a non-positional (2D) sound or music track tied to the object.\
"2D" means the playback is not spatialized: it does not change with the object's position relative
to the listener. For positional sound effects see [Audio (3D)](audio3d.md).

The referenced asset decides how the sound is played:

- **Samples** (`.wav` converted to `.tsw`) play as one-shot or looped SFX.
- **Sequences** (`.mid` / `.xm`) play as sequenced music. A MIDI needs a sound-font
  (`.sf2`) assigned, an XM brings its own.
- **Streamed audio** (`.mp3`, or `.wav` set to Opus/ULC compression) plays as
  streamed music decoded on the fly.

## Options

| Option | Description |
|--------|-------------|
| **Audio** | The audio asset to play (sample, sequence or stream). |
| **Sound-Font** | The sound-font asset, only shown for MIDI sequences. |
| **Volume** | Playback volume. |
| **Pitch** | Pitch offset in semitones (samples only, 12 = one octave up). |
| **Loop** | When enabled, the audio repeats when it reaches the end. A looping sample plays as a persistent SFX until stopped, and needs the "Loop" setting enabled on its asset (@TODO: auto-set). |
| **Auto-Play** | When enabled, playback starts automatically when the object spawns. |

## See also

- {cpp:struct}`P64::Comp::Audio2D`: the runtime component in the C++ API.
