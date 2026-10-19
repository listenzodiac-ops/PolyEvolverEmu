# Z-Synth

A JUCE (C++) VST3 / AU / Standalone synth inspired by the Dave Smith Instruments (Sequential) **Poly Evolver**. This is version 0.1: the core voice architecture, with the sequencer, modulation matrix and effects still to come.

This is an original, from-scratch synth inspired by the architecture and sound character of the Poly Evolver, not a sample-accurate clone. Z-Synth is the product name; change `PRODUCT_NAME` and `COMPANY_NAME` in `CMakeLists.txt` if you want something else.

## What's in it

| Section | What it does |
|---|---|
| **4 voices** | Polyphonic with voice stealing, like the Poly Evolver's 4 voices. |
| **Osc 1 + 2 (analog-style)** | PolyBLEP anti-aliased. *Shape* morphs triangle -> saw -> pulse, plus pulse width, semitone and fine tune. |
| **Osc 3 + 4 (digital)** | Band-limited wavetable oscillators with a 16-wave bank, morphed by the *Wave* knob. Stand-in for the Evolver's 128 digital waves. |
| **Filter** | Moog-style 4-pole saturating ladder, switchable 2-pole / 4-pole, with resonance, drive, key tracking and envelope amount. |
| **2 ADSR envelopes** | One for the filter, one for the amp. |
| **4 LFOs** | Sine / tri / saw / square / S&H. Per-voice. Destinations: pitch, osc shape, pulse width, cutoff, amp. |
| **Delay** | Stereo feedback delay with a tone filter in the feedback path. |
| **Master** | Volume plus tanh soft-clip safety. |
| **Step sequencer** | 16 steps, locked to the DAW: it starts when you press play and follows the host tempo and position (standalone app: internal 120 BPM clock). `Dest` chooses what the 16 knobs control: pitch (+/-12 semitones, snapped to notes), cutoff (+/-3 octaves) or pan (left to right). `Rate` is 1/4, 1/8, 1/16 or 1/32. `Gate` retriggers the envelopes on every step. |
| **Skins** | Blue and white, switched with the two swatches at the top right. Your choice is saved with the project. |

## Build on macOS

1. Install the tools (once):
   ```
   xcode-select --install
   brew install cmake
   ```
2. Configure and build (the first run downloads JUCE, so you need internet and git):
   ```
   cd ZSynth
   cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
   cmake --build build --config Release -j 8
   ```
3. The build copies the plugins into place automatically:
   - VST3: `~/Library/Audio/Plug-Ins/VST3/`
   - AU: `~/Library/Audio/Plug-Ins/Components/`
   - Standalone app: look under `build/ZSynth_artefacts/`

**First test:** launch the Standalone app and click the on-screen keyboard (or connect a MIDI keyboard in the app's audio/MIDI settings). If you hear a detuned saw pair with a filter sweep, the whole chain works. Then rescan plugins in your DAW.

**If your DAW won't load it:** ad-hoc sign the plugin so macOS stops blocking it:
```
codesign --force --deep -s - ~/Library/Audio/Plug-Ins/VST3/"Z-Synth.vst3"
```
For Logic (AU), validate with `auval -v aumu Pevo Ynam`.

## Code map (read in this order)

1. `Source/Params.h` / `Params.cpp`: every parameter (ID, range, default). **To add a knob, start here**, then add it to the editor.
2. `Source/SynthVoice.h`: the heart of the synth. `EvolverVoice::renderNextBlock` is the per-sample signal path: oscillators -> ladder filter -> amp envelope. `updateControl()` runs every 16 samples and applies LFOs, pitch and filter modulation.
3. `Source/WaveTables.h`: builds the digital wave bank additively at startup.
4. `Source/StereoDelay.h`: the delay effect.
5. `Source/PluginProcessor.*`: the glue between host, MIDI and DSP; saves and restores presets.
6. `Source/PluginEditor.*`: the UI. Panels are built from `Section`, `Knob` and `Choice` helpers.

## Known limitations / good next steps

- **Oscillators 1-2 triangle** is not anti-aliased (fine at normal pitches).
- **Filter** runs at the host sample rate with no oversampling, so extreme resonance at very high cutoffs can alias. Adding 2x oversampling is a good first upgrade.
- **Not yet implemented** from the real instrument: step sequencer, full modulation matrix, oscillator sync / FM / ring mod, high-pass filter, distortion, glide, and the real 128-wave digital table.
- **Presets**: the plugin saves and restores its state in your DAW project, but there is no preset browser yet.
- **UI** is functional and generic, with no hardware-style skin.
