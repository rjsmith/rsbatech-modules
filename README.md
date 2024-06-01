# orestes-one
VCVRack control from the Electra-One MIDI controller





## Development

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