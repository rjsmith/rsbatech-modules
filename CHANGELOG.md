## 2.0.0-beta8 2 Nov 2024

- Added custom parameter labels
- [FIX] Mitigate Electra One 3.7 beta firmware bug that only allows run lua sysex commands up to 80 bytes long
- [FIX] Prevented OSCSender spamming VCVRack log with "OscSender trying to send with empty socket" messages
- [FIX] Commented - out DEBUG statements
- [MAP] Mapped Sapphire modules
- [MAP] Mapped Squinktronix modules
- [DOC] Re-sized Wiki images

## 2.0.0-beta7 25 Oct 2024

- [MAP] Mapped Instruō modules
- [MAP] Mapped Synthesizer.com modules
- [FIX] Load default preset library or factory library when added to rack
## 2.0.0-beta6 19 Oct 2024

- Implemented the Pylades module for VCV Rack control from any TouchOSC device
- Added Pylades TouchOSC .tosc preset file
- Added factory module mappings for SurgeXT
- Added an optional "Scroll to selected module" rack navigation feature

## 2.0.0-beta5 29 Sep 2024

- Added module mappings for ProkModular premium plugin

## 2.0.0-beta4 21 Sep 2024

- Added more module mappings including the Vult free and premium plugins
- Reduced size of mapping library JSON text files by shortening JSON key strings and saving without pretty-printed spaces .. from 1.2Mb to 300Kb.

## 2.0.0-beta3 12 July 2024

- Added more module mappings to factory library (ChowDSP, Nonlinear Circuits, Valley)

## 2.0.0-alpha

- Control individual rack module parameters with rich value and control label feedback on the Electra-One screen
- Smooth 14-bit MIDI sysex controller changes
- Manual parameter mapping (from MIDI-CAT)
- Navigate between the mapped modules in the current VCVRack patch using Electra-One's touch screen
- Automap a single module
- Automap all modules in a rack in one go, optionally skipping or overwriting existing module mappings
- Mouse-free rack module selection and navigation using Electra One
- Special support designed for MindMeld PatchMaster modules in a rack, allowing switching between a single rack-level mapping and individual module mappings.
- Create and manage re-usable module-specific mappings stored in a module mapping library file
- Export and import module mappings, so they can be shared and curated by the user community (there are over 3000 modules to be mapped after all!)
- Factory - provided module mappings included