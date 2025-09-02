# NeuroLayer

Acquire data using one of the NeuroLayer probes developped by the Haesler lab. 

## Installation (currently Windows only)

*TBD*

## Usage

Instructions for using the plugin are available ... (*TBD*).

##### Important Note: Syncing analog and digital channels only works with NI devices that support correlated (hardware-timed) digital I/O (see docs above).

## Building from source

First, follow the instructions on [this page](https://open-ephys.github.io/gui-docs/Developer-Guide/Compiling-the-GUI.html) to build the Open Ephys GUI.

**Important:** This plugin is intended for use with the latest version of the GUI (1.0.0 and higher). The GUI should be compiled from the [`main`](https://github.com/open-ephys/plugin-gui/tree/main) branch, rather than the former `master` branch.

Then, clone this repository into a directory at the same level as the `plugin-GUI`, e.g.:

```
Code
├── plugin-GUI
│   ├── Build
│   ├── Source
│   └── ...
├── OEPlugins
│   └── NeuroLayerOEPlugin
│       ├── Builds
│       ├── Source
│       └── ...
```

### Windows

**Driver Installation** You must first install the NI-DAQmx drivers from [here](https://www.ni.com/en-us/support/downloads/drivers/download.ni-daq-mx.html#348669)

**Requirements:** [Visual Studio](https://visualstudio.microsoft.com/) and [CMake](https://cmake.org/install/)

From the `Build` directory, enter:

```bash
cmake -G "Visual Studio 17 2022" -A x64 ..
```

Next, launch Visual Studio and open the `OE_PLUGIN_ni-daqmx-plugin.sln` file that was just created. Select the appropriate configuration (Debug/Release) and build the solution.

Selecting the `INSTALL` project and manually building it will copy the `.dll` and any other required files into the GUI's `plugins` directory. The next time you launch the GUI from Visual Studio, the new plugin should be available.


## Attribution

This plugin was created by Marine Guyot for the VIB Haesler lab. It is inspired from the NIDAQX plugin developped by Open-Ephys team. 

If you need help getting this plugin to work with your hardware, please contact Marine at marine@codingresearcher.com. 