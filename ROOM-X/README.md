# ROOM-X by Piradex

Professional modal musical reverb for **macOS + Windows**, targeting **AU (Audio Unit v2)** and **VST3**.

## Product identity
ROOM-X combines a premium studio reverb with a 12-tone pitch-aware modal shaping layer. The user can emphasize or de-emphasize resonant energy according to notes, chords, scales, sequences or live MIDI input.

### Target color states
- **Early** -> coral/red accent
- **All** -> teal/green accent
- **Late** -> electric-blue accent

The whole accent system should transition smoothly when Target changes while the neutral graphite UI remains stable.

## Main modules
- Hybrid algorithmic + IR-derived room engine
- Early-reflection path + late-tail path
- Modal pitch tempering engine
- Manual note selection
- Chord/scale library
- Host-synced sequencer
- Live MIDI tempering + sustain pedal
- Note/pitch energy visualizer
- Spectrum analyzer with temper range and wet low/high cut
- Preset system with >= 60 factory presets and >= 11 tutorial presets
- Shared Beatfreak authentication / licensing integration
- GitHub Actions builds for macOS Universal and Windows x64

## Licensing
ROOM-X must reuse the existing Beatfreak authentication/licensing behavior rather than creating a separate account model. Network/auth operations must never run on the real-time audio thread. Use secure signed/opaque activation tokens and the shared device-registration/revocation policy.

## Important IP rule
The supplied Temperance Pro material is a workflow/UX study reference only. ROOM-X must use original DSP implementation, geometry, artwork, branding, preset names and source code. Do not copy Eventide assets, logos, source code, patented implementation details or exact visual trade dress.

## Docs
See `docs/ROOM-X_DEVELOPMENT_SPEC.md` for the implementation prompt and engineering requirements.
