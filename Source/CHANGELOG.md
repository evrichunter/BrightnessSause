# Changelog

## v2.0
- UI redesigned: carbon black, dark surfaces, matte gold. Sans-serif hierarchy (titles / parameter names / values), no ice animation.
- New dynamic de-esser (after Exciter + Air, before EQ): DE-ESS on/off, FREQUENCY 3-12 kHz, RANGE 0-6 dB, SENSITIVITY, LISTEN. Reduction readout next to the DE-ESS switch.
- EQ frequency ranges widened with true logarithmic knob mapping: PRESENCE FREQ 1-20 kHz (was 1-10), AIR FREQ 2-20 kHz (was 5-20).
- DSP chain, existing parameters, IDs, defaults, MIX/OUTPUT behaviour and zero latency unchanged.
- Projects saved with v1 load with the de-esser OFF so they sound exactly as before.

## Files modified
Source/DSP.h, Source/PluginProcessor.h, Source/PluginProcessor.cpp, Source/PluginEditor.h, Source/PluginEditor.cpp, Tests/dsp_test.cpp, README.md.
Unchanged: CMakeLists.txt, .github/workflows/build.yml.
