# DXVK

Provides a Vulkan-based implementation of DXGI and D3D11 in order to run 3D applications on Linux using Wine.

For the current status of the project, please refer to the [project wiki](https://github.com/doitsujin/dxvk/wiki).

## Build instructions

### Requirements:
- [wine-staging](https://wine-staging.com/) for Vulkan support
- [Meson](http://mesonbuild.com/) build system
- [MinGW64](http://mingw-w64.org/) compiler and headers
- [SDL2](https://www.libsdl.org/) headers and DLL

### Building DLLs
Inside the dxvk directory, run:
```
# 64-bit build. For 32-bit builds, replace
# build-win64.txt with build-win32.txt
meson --cross-file build-win64.txt build.w64
cd build.w64
meson configure -Dprefix=/target/directory
ninja
ninja install
```

Both `dxgi.dll` and `d3d11.dll`as well as some demo executables will be located in `/your/directory/bin`.

## How to use
In order to run `executable.exe` with DXVK,
* Copy `dxgi.dll`, `d3d11.dll` and `SDL2.dll` into the same directory as the executable
* Run `WINEDLLOVERRIDES=d3d11,dxgi=n wine executable.exe`

DXVK will create a file `dxgi.log` in the current working directory and may print out messages to stderr.

### Environment variables
The behaviour of DXVK can be modified with environment variables.

- `DXVK_SHADER_DUMP_PATH=directory` Writes all DXBC and SPIR-V shaders to the given directory
- `DXVK_DEBUG_LAYERS=1` Enables Vulkan debug layers. Highly recommended for troubleshooting and debugging purposes.

## Samples and executables
In addition to the DLLs, the following standalone programs are included in the project:

- `d3d11-compute`: Runs a simple compute shader demo. Requires native `d3dcompiler_47.dll`.
- `d3d11-triangle`: Renders a bunch of triangles using D3D11. Requires native `d3dcompiler_47.dll`.
- `dxgi-factory`: Enumerates DXGI adapters and outputs for debugging purposes.
- `dxbc-compiler`: Compiles a DXBC shader to SPIR-V.
- `dxbc-disasm`: Disassembles a DXBC shader. Requires native `d3dcompiler_47.dll`.
- `hlsl-compiler`: Compiles a HLSL shader to DXBC. Requires native `d3dcompiler_47.dll`.