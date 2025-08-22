# RSBATech-Modules

<!-- Version and License Badges -->
![Version](https://img.shields.io/badge/version-2.1.9-green.svg?style=flat-square)
![License](https://img.shields.io/badge/license-GPLv3+-blue.svg?style=flat-square)
![Language](https://img.shields.io/badge/language-C++-yellow.svg?style=flat-square)

A [VCV Rack](https://vcvrack.com/) Plugin.

This Plugin includes the following modules:

* Orestes-One (module parameter control from Electra One MIDI Controller)
* Pylades (module parameter control from TouchOSC)


## Orestes-One

Orestes-One (the [brother of Electra](https://en.wikipedia.org/wiki/Orestes) in Greek mythology) is a VCV Rack module to control modules and parameters in a rack with the [Electra-One MIDI controller](https://electra.one). It works together with a specific [Electra-One preset](https://app.electra.one/preset/4rIzUF8a60kXiYsyvlTN) to provide two-way control and parameter feedback using smooth 14bit value changes.  


![Orestes-One controlling a Befaco Rampage module](/images/OrestesOne.png?raw=true "Orestes-One controlling a Befaco Rampage module")

![Electra-One preset showing a module control page](/images/E1VCVRackPresetModule.png?raw=true "Electra-One preset showing a module control page")

![Electra-One preset showing module select page](/images/E1VCVRackPresetModuleGrid.png?raw=true "Electra-One preset showing the module select page")

## Pylades

Pylades (the [cousin of Orestes](https://en.wikipedia.org/wiki/Pylades) in Greek mythology) is a VCV Rack module to control modules and parameters in a rack with [TouchOSC](https://hexler.net/touchosc). It works together with a [specific TouchOSC preset](https://github.com/rjsmith/rsbatech-modules/tree/main/touchosc) to provide two-way control and parameter feedback using smooth 14bit value changes.  

![Pylades](/images/PyladesModule.png?raw=true "Pylades VCVRack module")

![Pylades TouchOSC preset showing a module control page](/images/TouchOSCMixMaster.png?raw=true "Pylades TouchOSC preset showing a module control page")

![Pylades TouchOSC preset  showing module select page](/images/TouchOSCModuleGrid.png?raw=true "Pylades TouchOSC preset showing the module select page")


## Key Features

Functionally, Orestes-One and Pylades are basically ([MIDI-CAT](https://library.vcvrack.com/Stoermelder-P1/MidiCat) minus MIDI CC7 & MIDI notes) + ([Oscelot](https://library.vcvrack.com/OSCelot/OSCelot) minus OSC) + bi-directional Electra-One SysEx or OSC commands plus some of my own ideas

* Control individual rack module parameters with rich value and control label feedback on the Electra-One / TouchOSC screen
* Smooth 14-bit value controller changes
* Manual parameter mapping (from MIDI-CAT)
* Navigate between the mapped modules in the current VCVRack patch using the touch screen of Electra One or TouchOSC.
* Optionally scroll the VCVRack window to the mapped module
* Automap a single module
* Automap all modules in a rack in one go, optionally skipping or overwriting existing module mappings
* Mouse-free rack module selection and navigation using Electra One or TouchOSC
* Special support designed for [MindMeld PatchMaster](https://library.vcvrack.com/MindMeldModular/PatchMaster) modules in a rack, allowing switching between a single rack-level mapping and individual module mappings.
* Create and manage re-usable module-specific mappings stored in a module mapping library file
* Override parameter labels and control page labels
* Export and import module mappings, so they can be shared and curated by the user community (there are over 3000 modules to be mapped after all!)
* [Factory - provided module mappings](https://github.com/rjsmith/rsbatech-modules/wiki/Factory-Module-Mapping-Library) included, almost 700 mappings in v2.1.9
* Use multiple Pylades and Orestes-One modules connected to different TouchOSC and Electra-One controllers in the same Rack (e.g. dedicate one iPad to controlling a Mix Master, another iPad for controlling other modules and an Electra-One controlling a Rack-wide mapping, all at the same time)

Plus most MIDI-CAT features (up to and including [v2.0.beta4](https://github.com/stoermelder/vcvrack-packone/blob/v2/CHANGELOG.md#20beta4))

## Documentation

Please see the [Wiki](https://github.com/rjsmith/rsbatech-modules/wiki) for the Pylades, Orestes-One and Electra-One preset user guide.

## Change History

Please see the [Change Log](CHANGELOG.md).

## Installation from github - built installers

Until this plugin is made available in the VCV Rack library:

1. Download the latest RSBATechModules installer for your platform from the [releases list](https://github.com/rjsmith/rsbatech-modules/releases) in this git repository
2. Copy the extracted .vcvrackplugin file into the Rack2 plugins folder for your machine's architecture e.g. on mac: ```/Users/<user>/Library/Application Support/Rack2/plugins-mac-arm64```.  Restart VCVRack and it should automatically unpack and install the module. 


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

At least 80% of the code of Orestes-One was directly copied from the [MIDI-CAT MIDI controller module](https://library.vcvrack.com/Stoermelder-P1/MidiCat) which is part of the essential [stoermelder PackOne](https://github.com/stoermelder/vcvrack-packone) VCV Rack Plugin. I literally could not have created this plugin without it.  The author has gracefully OK'd the use of the software in this plugin.

Thanks to TheModularMind for code, inspiration and ideas from the [Oscelot](https://library.vcvrack.com/OSCelot/OSCelot) module. 

Pylades (& Oscelot) uses code from the [oscpack project](https://github.com/RossBencina/oscpack). I modified the Pylades oscpack code to compile on the Mac ARM64 architecture.

## Development

Development notes, in case I forget.

### Bumping RSBATechModules Plugin Version

Both the Electra One and TouchOSC presets do a version handshake with a connected Oresets-One / Pylades VCVRack module when they start up. This is designed to ensure the combination of device preset + VCVRack module is supported.  It also allow for breaking changes in the API between the device and the VCCRack module ... the user will be prompted to upgrade their device preset if necessary.

For this to work, the device presets have hardcoded [semver](https://semver.org/) version strings which track the semantic version of the RSBATechModules VCvRack plugin.  The major and minor versions must match.  The semver patch level version does not need to match.  This also means that incrementing the minor version of the RSBATechModules plugin will require a new version of the device presets.

```lua
function isPyladesCompatibleWithTouchOSC(pyladesVersion)
  local semver = string.find(pyladesVersion, SEMVER_PATTERN)
  if (semver == nil) then return false end
  local pyladesMajorVersion, pyladesMinorVersion = string.match(pyladesVersion, SEMVER_PATTERN)
  return oscMajorVersion == pyladesMajorVersion and oscMinorVersion == pyladesMinorVersion
end
```

**Bumping RSBATechModules plugin patch-level version:**

Non-breaking API change

- [ ] Update ```version``` in plugin.json
- [ ] Update README.md Version badge 
- [ ] Update CHANGELOG.md

Finally: 

- [ ] Tag last commit with ```vX.Y.Z``` before pushing to github. (This will trigger the github actions to build a new release)

**Bumping RSBATechModules plugin minor-level version:**

Breaking API change in either Pylades or Orestes-One (probably usually both at same time)

- [ ] Repeat 1st set of patch-level version steps
- [ ] Edit pylades .tosc file ROOT script, change the ```local OSC_PRESET_VERSION = "2.0.0-beta7"``` version string, then commit updated .tosc file
- [ ] Edit Electra One preset Lua script, change the ```local E1_PRESET_VERSION <const> = "2.0.0"``` version string, then **Save Revision**

Finally: 

- [ ] Tag last commit with ```vX.Y.Z``` before pushing to github. (This will trigger the github actions to build a new release)

