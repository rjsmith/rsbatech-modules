# RSBATech-Modules

<!-- Version and License Badges -->
![Version](https://img.shields.io/badge/version-2.0.0--alpha1-green.svg?style=flat-square)
![License](https://img.shields.io/badge/license-GPLv3+-blue.svg?style=flat-square)
![Language](https://img.shields.io/badge/language-C++-yellow.svg?style=flat-square)

A [VCV Rack](https://vcvrack.com/) Plugin.

This Plugin includes the following modules:

* Orestes-One


## Orestes-One

Orestes-One is a VCV Rack module to control modules and parameters in a rack with the [Electra-One MIDI controller](https://electra.one). It works together with a specific [Electra-One preset](https://app.electra.one/preset/4rIzUF8a60kXiYsyvlTN) to provide two-way control and parameter feedback using smooth 14bit value changes.  


![Orestes-One controlling a Befaco Rampage module](/images/OrestesOne.png?raw=true "Orestes-One controlling a Befaco Rampage module")

![Electra-One preset showing a module control page](/images/E1VCVRackPresetModule.png?raw=true "Electra-One preset showing a module control page")

![Electra-One preset showing module select page](/images/E1VCVRackPresetModuleGrid.png?raw=true "Electra-One preset showing the module select page")

## Key Features

Functionally, Orestes-One is basically ([MIDI-CAT](https://library.vcvrack.com/Stoermelder-P1/MidiCat) minus MIDI CC7 & MIDI notes) + ([Oscelot](https://library.vcvrack.com/OSCelot/OSCelot) minus OSC) + bi-directional Electra-One SysEx + some of my own ideas

* Control individual rack module parameters with rich value and control label feedback on the Electra-One screen
* Smooth 14-bit MIDI sysex controller changes
* Manual parameter mapping (from MIDI-CAT)
* Navigate between the mapped modules in the current VCVRack patch using Electra-One's touch screen 
* Automap a single module
* Automap all modules in a rack in one go, optionally skipping or overwriting existing module mappings
* Mouse-free rack module selection and navigation using Electra One
* Special support designed for [MindMeld PatchMaster](https://library.vcvrack.com/MindMeldModular/PatchMaster) modules in a rack, allowing switching between a single rack-level mapping and individual module mappings.
* Create and manage re-usable module-specific mappings stored in a module mapping library file
* Export and import module mappings, so they can be shared and curated by the user community (there are over 3000 modules to be mapped after all!)
* [Factory - provided module mappings](https://github.com/rjsmith/rsbatech-modules/wiki/Orestes%E2%80%90One-Module-User-Guide#factory-mapping-library) included

Plus most MIDI-CAT features (up to [v2.0.beta4](https://github.com/stoermelder/vcvrack-packone/blob/v2/CHANGELOG.md#20beta4))

## Documentation

Please see the [Wiki](https://github.com/rjsmith/rsbatech-modules/wiki) for the Orestes-One and Electra-One preset user guide.

## Change History

Please see the [Change Log](CHANGELOG.md).

## Installation from repo - built installers

Until this plugin is made available in the VCV Rack library, download the latest RSBATechModules installer for your platform from the releases list in this git repository, and copy the extracted .vcvrackplugin file into the Rack2 modules folder for your machine's architecture e.g. ```plugins-mac-arm64```.  Restart VCV rack and it should unpack and install the module. 


## Building & Installation from Source Code


To build the plugin, download and unzip the [VCVRack SDK](https://vcvrack.com/downloads) for your machine's architecture and target VCVRack version.

Then run (change path to SDK folder):

```
 RACK_DIR=~/Projects/Rack-SDK-arm64 make install
```

If everything goes OK, a new .vcvplugin file is created in your installed VCVRack Rack2 folder.  Now just stop and start VCVRack and the plugin will load its modules.

You can open a terminal window and view the VCVRack log to help debug any issues

```
cd <path to rack2 folder>
tail -f log.txt
```

## Licenses

The software within the src/ folder is licensed under GPL 3+ (see LICENSE for more details)

The UI elements within the res/ folder are licensed under [CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/). You may not distribute modified adaptations of these graphics.

## Acknowledgements

At least 80% of the code of Orestes-One was directly copied from the [MIDI-CAT MIDI controller module](https://library.vcvrack.com/Stoermelder-P1/MidiCat) which is part of the essential [stoermelder PackOne](https://github.com/stoermelder/vcvrack-packone) VCV Rack Plugin. 

Thanks to TheModularMind for code and ideas from the [Oscelot](https://library.vcvrack.com/OSCelot/OSCelot) module. 