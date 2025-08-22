## 2.1.9 22 Aug 2025

- [FIX] Clear/reset control page labels when starting a new module mapping
- [MAP] Mapped new CV Funk plugin modules v2.0.24
- [MAP] Mapped Venom Oscillators (Sofias Daughter) plugin v2.1.8
- [MAP] Mapped new Sapphire plugin modules v2.6.0
- [MAP] Mapped Synthesis Technology Bundle 2 (E352 & E370) v2.1.0

## 2.1.8 6 June 2025

- Removed the previous horizontal fader-based TouchOSC preset from the repository. The "Pylades vx.x.x.tosc" file is now the knob-based TouchOSC layout. 
  - I always intended to remove the horizontal fader - based layout, I think the knob layout with its vertical finger movements is more natural to use, especially when controlling 2+ parameters at the same time.
- Added custom module / rack mapping control page labels ("Set control page names" option)
  - Use the updated Pylades v2.1.3.tosc TouchOSC file to see the new dynamic control page labels.
  - The Electra One VCVRack preset has already been updated to support page names (v2.1.5)
- Ported [PackOne ScaledMapParam snapEnabled control improvement](https://github.com/stoermelder/vcvrack-packone/commit/3369bec1bf2eb6ba5878f382f76da704fb32829e)
- [MAP] Added control page names to MindMeld Mixers and AuxExpander module mappings
- [MAP] Mapped Cella plugin v2.9.0
- [MAP] Mapped MUS-X plugin v2.1.1
  - NB: Set the "Mod Matrix" module's "Latch buttons" context option = true


## 2.1.7 16 May 2025

- [FIX] Prevent crash by replacing OscReceiver queue with a lock-free threadsafe dsp::RingBuffer

## 2.1.6 11 May 2025

- [MAP] Mapped NANO modules v2.3.9 (OCTA)
- [MAP] Mapped Moffenzeef plugin v2.0.3
- [MAP] Mapped nullpath plugin v2.0.1
- [MAP] Mapped Sanguine Mutants v2.6.6  

## 2.1.5 2 Mar 2025

- [MAP] Mapped all 4MS plugin v2.0.11
- [MAP] Mapped CV Funk plugin modules v2.0.12
- [MAP] Mapped Bastl Pizza plugin modules v2.1.1

## 2.1.4 26 Jan 2025

- [FIX] Prevent crash if access save mapping context menu after deleting mapped module from the rack (and mappings not automatically removed)
- [MAP] Mapped Cytomic plugin v2.0.6
- [MAP] Mapped Jasmine & Olive Trees plugin v2.0.2
- [MAP] Mapped some NYSTHI modules - Simpliciter & Wormholizer v2.4.23

## 2.1.3 15 Jan 2025

- [MAP] Mapped UnfilteredVolume1 plugin modules v2.2.1

## 2.1.2 5 Jan 2025

- [FIX] Prevent non-ASCII module names (e.g. from Instruō) from being encoded and sent to E1 (which does not support unicode)
- [MAP] Fixed Audible Instruments Elements module mapping

## 2.1.1 29 Nov 2024

- [FIX] Downgraded OscMessage FATAL to WARN log, to prevent crashing VCVRack
- [MAP] Mapped new NANO plugin modules v2.3.8.2
- [MAP] Mapped new Befaco plugin modules v2.8.2
- [MAP] Mapped new Hetrick plugin modules v2.5.0

## 2.1.0 24 Nov 2024

- Refactored Pylades OSC message names and arguments, hence need to bump minor version of plugin
- [MAP] Added Venom plugin mappings
- [MAP] Added Hetrick plugin mappings

## 2.0.0-beta9 10 Nov 2024

- Added "Create empty mapping library" menu action
- Exported individual plugin mapping files into presets folder
- [FIX] Prevent crash reading a json file without a "plugin" top-level object property

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