# DXVK

A Vulkan-based compatibility layer for Direct3D 11 which allows running 3D applications on Linux using Wine.

For the current status of the project, please refer to the [project wiki](https://github.com/doitsujin/dxvk/wiki).

For binary releases, see the [releases](https://github.com/doitsujin/dxvk/releases) page.

## Build instructions

### Requirements:
- [wine-vulkan](https://github.com/roderickc/wine-vulkan) for Vulkan support
- [Meson](http://mesonbuild.com/) build system (at least 0.43)
- [MinGW64](http://mingw-w64.org/) compiler and headers (requires threading support)
- [glslang](https://github.com/KhronosGroup/glslang) front end and validator

### Building DLLs
Inside the dxvk directory, run:
```
# 64-bit build. For 32-bit builds, replace
# build-win64.txt with build-win32.txt
meson --cross-file build-win64.txt build.w64
cd build.w64
meson configure -Dprefix=/your/dxvk/directory/
# for an optimized release build:
meson configure -Dbuildtype=release
ninja
ninja install
```

The two libraries `dxgi.dll` and `d3d11.dll`as well as some demo executables will be located in `/your/dxvk/directory/bin`.

## How to use
In order to set up a wine prefix to use DXVK instead of wined3d globally, run:
```
cd /your/dxvk/directory/bin
WINEPREFIX=/your/wineprefix bash setup_dxvk.sh
```

Verify that your application uses DXVK instead of wined3d by checking for the presence of the log files `d3d11.log` and `dxgi.log` in the application's directory, or by enabling the HUD (see notes below).

### Notes on Vulkan drivers
Before reporting an issue, please check the [Wiki](https://github.com/doitsujin/dxvk/wiki/Driver-support) page on the current driver status.

### Online multi-player games
Manipulation of Direct3D libraries in multi-player games may be considered cheating and can get your account **banned**. This may also apply to single-player games with an embedded or dedicated multiplayer portion. **Use at your own risk.**

### Environment variables
The behaviour of DXVK can be modified with environment variables.

- `DXVK_DEBUG_LAYERS=1` Enables Vulkan debug layers. Highly recommended for troubleshooting and debugging purposes.
- `DXVK_SHADER_DUMP_PATH=directory` Writes all DXBC and SPIR-V shaders to the given directory
- `DXVK_SHADER_READ_PATH=directory` Reads SPIR-V shaders from the given directory rather than using the shader compiler.
- `DXVK_SHADER_VALIDATE=1` Enables SPIR-V shader validation. Useful for debugging purposes.
- `DXVK_SHADER_OPTIMIZE=1` Enables SPIR-V shader optimization. Experimental, use with care.
- `DXVK_LOG_LEVEL=error|warn|info|debug|trace` Controls message logging.
- `DXVK_HUD=1` Enables the HUD

## Samples and executables
In addition to the DLLs, the following standalone programs are included in the project.
Most of them require a native `d3dcompiler_47.dll`, which you can retrieve from your
Windows installation in case you have one.

- `d3d11-compute`: Runs a simple compute shader demo.
- `d3d11-triangle`: Renders a bunch of triangles using D3D11.
- `dxgi-factory`: Enumerates DXGI adapters and outputs for debugging purposes.
- `dxbc-compiler`: Compiles a DXBC shader to SPIR-V.
- `dxbc-disasm`: Disassembles a DXBC shader.
- `hlsl-compiler`: Compiles a HLSL shader to DXBC.

## Troubleshooting
DXVK requires threading support from your mingw-w64 build environment. If you
are missing this, you may see "error: 'mutex' is not a member of 'std'". On
Debian, this can usually be resolved by using the posix alternate, which
supports threading. For example, choose the posix alternate from these
commands (use i686 for 32-bit):
```
update-alternatives --config x86_64-w64-mingw32-gcc
update-alternatives --config x86_64-w64-mingw32-g++
```
