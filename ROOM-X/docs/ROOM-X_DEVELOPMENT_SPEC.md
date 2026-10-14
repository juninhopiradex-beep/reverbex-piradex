# ROOM-X by Piradex — Development Specification

## Goal
Build a production-ready **musical modal reverb** for **macOS AU + VST3 (Universal 2)** and **Windows VST3 x64**, using JUCE/C++. ROOM-X must be an original Beatfreak/Piradex product. The supplied Temperance Pro material is reference for workflow/control taxonomy only: do not copy Eventide source code, assets, exact geometry, preset names, trade dress, patented implementation details, or proprietary modal-conversion technology.

## Core architecture
Recommended signal flow:

`Input -> PreDelay -> Early Reflections -> Late Reverb/FDN -> Musical Modal Shaping -> Wet Filters -> Output/Mix`

Use an original hybrid engine: sparse/decorrelated early reflections, a stable multi-channel FDN or equivalent late-tail network, and an independent resonator/modal shaping layer. For original/licensed IR spaces, an offline modal-fitting tool may extract stable peaks/decays into ROOM-X proprietary space data. No allocation, locks, network, disk I/O, or GUI calls in the audio callback.

## Musical controls
- **Temper**: -100…+100, default 0. Positive emphasizes selected pitch regions; negative de-emphasizes them. At 0 ROOM-X behaves like a premium conventional reverb.
- **Note Width**: 1–99%, ~50% default. Narrow = pure/focused; wide = richer/broader modal field.
- **Target**: EARLY / ALL / LATE, selecting which temporal portion receives musical tempering.
- **Temper Range**: low/high musical/frequency boundary; outside region remains untempered.
- Global tuning: A4 reference 400–500 Hz, default 440 Hz; fine tune -50…+50 cents.

### Target color system
Base UI stays dark graphite. Accent state changes smoothly (~150–300 ms):
- EARLY -> coral/red (`#FF667A` family)
- ALL -> teal/green (`#40E0C0` family)
- LATE -> electric blue (`#3DA8FF` family)

Accent applies to central ring, NoteScape energy, selected arcs, spectrum highlights and range handles — not the entire interface.

## Note selection
### Manual
12 chromatic note toggles; Clear; Select All; semitone Transpose; note lock; Choose Scale browser with roots, intervals, chords, scales and modes.

### Sequencer
Host-synced programmable harmonic sequencer: up to 8 rows; 1–64 bars; beat grid; session offset; transpose; retrigger; host arm/follow; click-drag regions; copy/delete/move/resize; navigator/zoom; deterministic loop/offline bounce behavior.

### MIDI
MIDI note on/off, all channels by default, sustain CC64, input activity meter, Octave Repeat, fast click-free chord transitions, register-aware mode when Octave Repeat is disabled.

## Main reverb controls
- Pre-delay: 0–2000 ms, free or host sync
- Mix: 0–100%, equal-power, Mix Lock
- Decay: approximately 0.01x–30x base response or equivalent RT60 display
- Size: 50–200%
- Position: 99 Early … center … 99 Late
- Density: 1–100%
- Offset: -500…+500 Hz
- Input / Output: -60…+12 dB with meters
- Kill Dry
- Dual Mono
- Bypass, click-free and host-safe

## Wet filters and visualization
- Wet low-cut / high-cut: 20 Hz–20 kHz, 12 or 36 dB/oct
- Spectrum analyzer: wet output, log 20 Hz–20 kHz, pitch-region highlights, neutral outside Temper Range
- **NoteScape**: original 12-sector circular pitch-energy display around Temper; selected notes stay visually distinct; positive temper grows selected energy, negative temper produces visible recesses/holes
- Analysis/UI refresh must run outside the audio callback and degrade gracefully under load

## Space library
Ship 29 original spaces across:
1. Rooms
2. Halls
3. Mechanical
4. Unusual
5. Special FX

Use original/licensed IRs and original names only. Each space should define early model, late topology/data, default size, RT60, damping, stereo characteristics, density behavior and optional modal metadata.

## Presets
Minimum **60 factory presets + 11 tutorial presets**. Banks should cover vocals, drums, instruments, synths, mix FX, sound design, modal chords, sequenced spaces and MIDI performance. Tutorial presets must teach: normal reverb, positive/negative temper, narrow/wide width, Early/Late targets, Manual scale, Sequencer, MIDI, Density/Offset.

## Workflow utilities
Undo/Redo, previous/next preset, preset browser, Save/Save As/Import, A/B with copy both directions, Eco mode, info/settings, sharps/flats preference, note lock, mix lock and bypass.

## UI structure
Suggested resizable base ~1180x760 logical px:
- Top: ROOM-X branding, undo/redo, preset navigation, A/B, settings, Eco
- Main left: input, pre-delay, Mix, Decay, Size
- Main center: NoteScape + large Temper
- Main right: Target, Position, Density, Offset, output
- Middle: full-width wet spectrum with Temper Range + wet filter handles
- Lower: Manual / Sequencer / MIDI panels
- Bottom: status/version/license/bypass

Use original Piradex/Beatfreak typography, icons, geometry and branding.

## Authentication / license behavior
Reuse the established Piradex plugin logic:
- login overlay blocks UI until authorized
- **MASTER** credential: permanent, no expiry
- **BETA** credential: expiry date/status visible
- optional **Remember this computer** machine-bound local authorization
- never store/commit plaintext passwords
- preserve PBKDF2-HMAC-SHA256 salted-hash pattern (current family convention: 60,000 iterations)
- password matching off the real-time audio thread
- unauthorized plugin must remain **audio-transparent** so old DAW sessions are never muted or altered
- abstract behind `ILicenseProvider` so a future Beatfreak account/cloud provider can replace local-password logic
- never embed server secrets/API private keys in the client or repository

## Real-time rules
- no allocation, mutexes, filesystem, network or GUI calls in `processBlock`
- preallocate work/FFT buffers
- parameter smoothing for all audible changes
- denormal protection
- lock-free/atomic audio->UI meters
- support common sample rates 44.1–192 kHz and blocks roughly 16–4096
- report exact host latency if any convolution/oversampling mode introduces it
- target 0-sample algorithmic latency where practical; do not falsely advertise <1 ms for modes that require more

## State / parameters
Use APVTS or equivalent with immutable IDs and versioned migration. Save all key parameters, note selections, sequence, tuning, locks, chosen space and appropriate UI utility state. Complex sequence cells may be stored in versioned state instead of exposing hundreds of automation parameters.

## Suggested source tree
```text
ROOM-X/
  CMakeLists.txt
  README.md
  docs/
  Source/
    PluginProcessor.*
    PluginEditor.*
    dsp/        EarlyReflections, FDNReverb, ModalBank, MusicalTemper, WetFilters
    music/      PitchMap, ScaleLibrary, ChordLibrary, SequenceEngine, MidiNoteState, Tuning
    ui/         NoteScape, SpectrumView, panels, meters, TopBar, LoginOverlay
    state/      Parameters, PresetManager, StateMigration
    licensing/  License, MachineBinding
  tests/
  tools/
  .github/workflows/
```

## CI / release
macOS: Release AU+VST3 Universal 2, architecture check, `auval`, pluginval, tests, `.pkg`/`.dmg`, optional signing/notarization via GitHub Secrets.

Windows: Release x64 VST3, pluginval, tests, installer to `C:\Program Files\Common Files\VST3`.

Never store signing certificates/private keys in source control.

## Acceptance criteria
ROOM-X v1 is complete only when AU/VST3 Mac Universal and Windows VST3 build; auval/pluginval pass; Master/Beta login works; unauthorized audio is transparent; 29 original spaces exist; >=60 + 11 tutorial presets exist; Manual/Sequencer/MIDI operate; Early/All/Late targeting and dynamic colors work; Temper/Width/Range/Position/Density/Offset work; visualizers are safe; state restores correctly; no audio-thread allocation/locks; host/sample-rate/block-size regression testing passes; and CI creates release artifacts.

## Implementation sequence
1. Skeleton + state + CI
2. Core reverb
3. Musical modal engine
4. Manual musical UI
5. NoteScape + spectrum + color states
6. Sequencer + MIDI
7. Spaces + presets
8. Licensing + installers
9. Host/performance regression
10. Release candidate/manuals
