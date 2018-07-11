# DXVK

A Vulkan-based compatibility layer for Direct3D 11 which allows running 3D applications on Linux using Wine.

For the current status of the project, please refer to the [project wiki](https://github.com/doitsujin/dxvk/wiki).

For binary releases, see the [releases](https://github.com/doitsujin/dxvk/releases) page.

For Direct3D 10 support, check out [DXUP](https://github.com/Joshua-Ashton/dxup), which can be used together with DXVK.

## Build instructions

### Requirements:
- [wine 3.10](https://www.winehq.org/) or newer
- [Meson](http://mesonbuild.com/) build system (at least version 0.43)
- [MinGW64](http://mingw-w64.org/) compiler and headers (requires threading support)
- [glslang](https://github.com/KhronosGroup/glslang) front end and validator

### Building DLLs

#### The simple way
Inside the DXVK directory, run:
```
./package-release.sh master /your/target/directory --no-package
```

This will create a folder `dxvk-master` in `/your/target/directory`, which contains both 32-bit and 64-bit versions of DXVK.

#### Compiling manually
```
# 64-bit build. For 32-bit builds, replace
# build-win64.txt with build-win32.txt
meson --cross-file build-win64.txt --prefix /your/dxvk/directory build.w64
cd build.w64
meson configure 
# for an optimized release build:
meson configure -Dbuildtype=release
ninja
ninja install
```

The two libraries `dxgi.dll` and `d3d11.dll`as well as some demo executables will be located in `/your/dxvk/directory/bin`.

## How to use
In order to set up a wine prefix to use DXVK instead of wined3d globally, run:

```
cd /your/compiling/directory/x32/
WINEPREFIX=/your/wineprefix bash setup_dxvk.sh
cd ../x64/
WINEPREFIX=/your/wineprefix bash setup_dxvk.sh
```

When built line by line, run:
```
cd /your/dxvk/directory/bin
WINEPREFIX=/your/wineprefix bash setup_dxvk.sh
```

Verify that your application uses DXVK instead of wined3d by checking for the presence of the log files `d3d11.log` and `dxgi.log` in the application's directory, or by enabling the HUD (see notes below).

### Notes on Vulkan drivers
Before reporting an issue, please check the [Wiki](https://github.com/doitsujin/dxvk/wiki/Driver-support) page on the current driver status.

### Online multi-player games
Manipulation of Direct3D libraries in multi-player games may be considered cheating and can get your account **banned**. This may also apply to single-player games with an embedded or dedicated multiplayer portion. **Use at your own risk.**

### HUD
The `DXVK_HUD` environment variable controls a HUD which can display the framerate and some stat counters. It accepts a comma-separated list of the following options:
- `devinfo`: Displays the name of the GPU and the driver version.
- `fps`: Shows the current frame rate.
- `frametimes`: Shows a frame time graph.
- `submissions`: Shows the number of command buffers submitted per frame.
- `drawcalls`: Shows the number of draw calls and render passes per frame.
- `pipelines`: Shows the total number of graphics and compute pipelines.
- `memory`: Shows the amount of device memory allocated and used.
- `version`: Shows DXVK version.

Additionally, `DXVK_HUD=1` has the same effect as `DXVK_HUD=devinfo,fps`.

### Debugging
The following environment variables can be used for **debugging** purposes.
- `VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_standard_validation` Enables Vulkan debug layers. Highly recommended for troubleshooting rendering issues and driver crashes. Requires the Vulkan SDK to be installed on the host system.
- `DXVK_CUSTOM_VENDOR_ID=<ID>` Specifies a custom PCI vendor ID
- `DXVK_CUSTOM_DEVICE_ID=<ID>` Specifies a custom PCI device ID
- `DXVK_LOG_LEVEL=none|error|warn|info|debug` Controls message logging
- `DXVK_FAKE_DX10_SUPPORT=1` Advertizes support for D3D10 interfaces
- `DXVK_USE_PIPECOMPILER=1` Enable asynchronous pipeline compilation. This currently only has an effect on RADV in mesa-git.

## Troubleshooting
DXVK requires threading support from your mingw-w64 build environment. If you
are missing this, you may see "error: 'mutex' is not a member of 'std'". On
Debian and Ubuntu, this can usually be resolved by using the posix alternate, which
supports threading. For example, choose the posix alternate from these
commands (use i686 for 32-bit):
```
update-alternatives --config x86_64-w64-mingw32-gcc
update-alternatives --config x86_64-w64-mingw32-g++
```
