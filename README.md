# DXVK - Amazing Project :)

An Vulkan-based translation layer for Direct3D 9/10/11 that allows running 3D applications on Linux using Wine.

For the current status of the project, please refer to the [project folder](https://github.com/doitsujin/dxvk/wiki).


## How to use
In order to install an DXVK package obtained from the [release](https://github.com/doitsujin/dxvk/releases) page into a given wine prefix, run the following commands from within the DXVK directory:

```
export WINEPREFIX=/path/to/.wine-prefix
./setup_dxvk.sh install
```

This will ** copy ** the DLLs to the `system32` and` syswow64` directories for the wine prefix and set the required DLL overrides. 32-bit prefixes are also supported.

The installation text optionally includes the following arguments:
- `--symlink`: create symbolic links to DLL files instead of copying. This is especially useful for development.
- `--with-d3d10`: Install help libraries` d3d10 {_1} .dll`.
- `- don-dxgi`: Do not install the DXVK DXGI application and use the wine-supplied application instead. This is necessary for vkd3d and DXVK to work with the same wine prefix.

Verify that the user of your application is DXVK instead of wined3d by checking that the application directory contains the log file `d3d9.log` or` d3d11.log`, or enable HUD (see notes below.)

To remove a DXVK from a prefix, do the following:
```
export WINEPREFIX=/path/to/.wine-prefix
./setup_dxvk.sh uninstall
```

## Build instructions

### Requirements:
- [came 3.10](https://www.winehq.org/) or newer
- [Meson](http://mesonbuild.com/) build system (at least version 0.46)
- [Mingw-w64](http://mingw-w64.org/) compiler and headers (at least version 6.0)
- [glslang](https://github.com/KhronosGroup/glslang) compiler

### Building DLLs

#### The simplest way
Inside the DXVK directory, run:
```
./package-release.sh master /your/target/directory --no-package
```

This will create a folder `dxvk-master` in `/your/target/directory`, which contains both 32-bit and 64-bit versions of DXVK, which can be set up in the same way as the release versions as noted above.

In order to preserve the build directories for development, pass `--dev-build` to the script. This option implies `--no-package`. After making changes to the source code, you can then do the following to rebuild DXVK:
```
# change to build.32 for 32-bit
cd /your/target/directory/build.64
ninja install
```

#### Compiling manually
```
# 64-bit build. For 32-bit builds, replace
# build-win64.txt with build-win32.txt
meson --cross-file build-win64.txt --buildtype release --prefix /your/dxvk/directory build.w64
cd build.w64
ninja install
```

The D3D9, D3D10, D3D11 and DXGI DLLs will be located in `/your/dxvk/directory/bin`. Setup has to be done manually in this case.

### Notes on Vulkan drivers
Before reporting an issue, please check the [Wiki](https://github.com/doitsujin/dxvk/wiki/Driver-support) page on the current driver status and make sure you run a recent enough driver version for your hardware.

### Online multi player games
Manipulation of Direct3D libraries in multi-player games may be considered cheating and can get you account **banned**. This may also apply to single player games with an embedded or dedicated multi player portion. **Use at you own risk.**

### HUD
The `DXVK_HUD` environment variable controls a HUD which can display the framerate and some stat counters. It accepts a comma-separated list of the following options:
- `devinfo`: Displays the name of the GPU and the driver version.
- `fps`: Shows the current frame rate.
- `frametimes`: Shows a frame time graph.
- `submissions`: Shows the number of command buffers submitted per frame.
- `drawcalls`: Shows the number of draw calls and render passes per frame.
- `pipelines`: Shows the total number of graphics and compute pipelines.
- `memory`: Shows the amount of device memory allocated and used.
- `gpuload`: Shows estimated GPU load. May be inaccurate.
- `version`: Shows DXVK version.
- `api`: Shows the D3D feature level used by the application.
- `compiler`: Shows shader compiler activity
- `samplers`: Shows the current number of sampler pairs used *[D3D9 Only]*

Additionally, `DXVK_HUD=1` has the same effect as `DXVK_HUD=devinfo,fps`, and `DXVK_HUD=full` enables all available HUD elements.

### Device filter
Some applications do not provide a method to select a different GPU. In that case, DXVK can be forced to use a given device:
- `DXVK_FILTER_DEVICE_NAME="Device Name"` Selects devices with a matching Vulkan device name, which can be retrieved with tools such as `vulkaninfo`. Matches on substrings, so "VEGA" or "AMD RADV VEGA10" is supported if the full device name is "AMD RADV VEGA10 (LLVM 9.0.0)", for example. If the substring matches more than one device, the first device matched will be used.

**Note:** If the device filter is configured incorrectly, it may filter out all devices and applications will be unable to create a D3D device.

### State cache
DXVK caches pipeline state by default, so that shaders can be recompiled ahead of time on subsequent runs of an application, even if the driver's own shader cache got invalidated in the meantime. This cache is enabled by default, and generally reduces stuttering.

The following environment variables can be used to control the cache:
- `DXVK_STATE_CACHE=0` Disables the state cache.
- `DXVK_STATE_CACHE_PATH=/some/directory` Specifies a directory where to put the cache files. Defaults to the current working directory of the application.

### Debugging
The following environment variables can be used for **debugging** purposes.
- `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` Enables Vulkan debug layers. Highly recommended for troubleshooting rendering issues and driver crashes. Requires the Vulkan SDK to be installed on the host system.
- `DXVK_LOG_LEVEL=none|error|warn|info|debug` Controls message logging.
- `DXVK_LOG_PATH=/some/directory` Changes path where log files are stored. Set to `none` to disable log file creation entirely, without disabling logging.
- `DXVK_CONFIG_FILE=/xxx/dxvk.conf` Sets path to the configuration file.

## Troubleshooting
DXVK requires threading support from your mingw-w64 build environment. If you
are missing this, you may see "error: 'mutex' is not a member of 'std'". 

On Debian and Ubuntu, this can be resolved by using the posix alternate, which
supports threading. For example, choose the posix alternate from these
commands (use i686 for 32-bit):
```
update-alternatives --config x86_64-w64-mingw32-gcc
update-alternatives --config x86_64-w64-mingw32-g++
```
For non-Debian based distros, make sure that you mingw-w64-gcc cross compiler 
do have `--enable-threads=posix` enabled during configure. If your distro does
ship its mingw-w64-gcc binary with `--enable-threads=win32` you might have to
recompile locally or open a bug at your distro's bugtracker to ask for it. 
