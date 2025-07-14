# DXVK LOW-LATENCY

Enhances the original [dxvk](https://github.com/doitsujin/dxvk) with low-latency frame pacing capabilities to improve game responsiveness and input lag. It also improves latency stability over time, usually resulting in a more accurate playback speed of the generated video.

### Installation

The most common way to install is to replace the dxvk .dll files within the Proton directory. You can theoretically do this with Steam's Proton, but since Steam will periodically update Proton, it will regularly overwrite the .dll files, and thus it's instead recommended to put a Proton version into `$HOME/.local/share/Steam/compatibilitytools.d`, such as [Proton-GE](https://github.com/GloriousEggroll/proton-ge-custom) where you can find the .dll files in `files/lib/wine/dxvk`, `i386-windows` for 32 bit and `x86_64-windows` for 64 bit. Depending on the particular Proton version, the folders might be structured slightly differently. This Proton version then can be selected in Steam, Lutris, Heroic, etc.

When using dxvk low-latency in Wine directly, see [How to use](https://github.com/netborg-afps/dxvk/?tab=readme-ov-file#how-to-use).

### Options

#### dxvk.framePace

dxvk low-latency is configured such that it achieves its goal without setting any options. The exception to this currently is that the user might want to enable the VRR mode manually. Fine-tuning the pacing via options is also possible.

The config variable `dxvk.framePace` in `dxvk.conf` can be set to

- `"max-frame-latency"` is the behaviour of upstream dxvk. Frame `i` won't start as long as frame `(i-1)-x` isn't finished, where `x` is the value of `dxgi.maxFrameLatency` / `d3d9.maxFrameLatency`. This pacing usually looks smooth, but has latency issues when GPU bound. Optimized for highest fps.
- `"min-latency"` is essential like `max-frame-latency-0` (not selectable for the mode above), which means the start of a frame will wait until the previous one is finished. CPU/GPU no longer overlap during the transition from one frame to another and thus a lot of fps are sacrificed for prioritizing low latency. This mode is generally not recommended, but might be useful to get insights.
- `"low-latency"` is the default mode: It combines high fps throughput with excellent game responsiveness and low input lag. Looking at a scale of a few seconds, pacing is usually more accurate in time than `max-frame-latency` since latency variations are minimized, especially when moving in and out of the GPU limit and when GPU frametimes vary a lot while being GPU bound. Looking at the pacing frame by frame, this mode relies on the game providing stable frame times for smoothness. When the game generates occasional stutters, these are filtered out nicely such that they don't interfere with the presentation of the other frames.
- `"low-latency-vrr-240"` enhances the above mode by taking v-blank information into account, which prevents additional v-sync buffering latency. This mode automatically enables v-sync to get informed when v-blanks are happening. It will make the pacer predict future v-blanks based on the given refresh rate of 240 Hz. Replace 240 with the refresh rate of your monitor. This mode works with x11-flip and native Wayland (can be enabled in Wine via `DISPLAY= `), but cannot work on Xwayland because v-blank information is not available there (tested on Nvidia). Care has to be taken that the system is configured such that the display is indeed using a variable refresh rate, otherwise this mode won't work properly.

Setting the frame pacing via the environment variable `DXVK_FRAME_PACE` is also supported.

#### d3d9/dxgi.maxFrameRate

Fps limiting is very helpful when used together with VRR to give the pacing enough space to not hit the v-sync buffering. For 240 Hz VRR, a limit of about 225 fps is recommended, and similarly for other refresh rates. Finding the best fps-limit for VRR will require some testing and also depends on the game and how much frametime variation it generates on the user's system. 

Fps limiting is also useful in other modes to improve consistency and/or to save power. Limiting in the low-latency modes is tightly integrated into the frame pacing and is strongly recommended to be used in place of most ingame limiters. 

If setting an fps limit so low that it will bore your GPU, this may result in your driver's heuristic clocking down the GPU, which can cause the latency to quadruple if you are unlucky. So you may want to watch out for that and take appropriate measures if desired.

Setting the fps limit via the environment variable `DXVK_FRAME_RATE` is also supported.

#### dxvk.hud

Latencies can be visualized by adding the following to `dxvk.hud`/`DXVK_HUD`. Average over 100 frames.

- `renderlatency`: start of frame (usually when the game starts processing input) until the GPU did finish rendering this frame. Note that this will not work when a game's fps limiter is enabled, as there is no way to detect when a game will stall processing before reading input.
- `presentlatency`: time it takes to present the finished image to the screen. Relies on the driver implementation of `vkWaitForPresentKHR`, which may or may not be accurate. `VK_PRESENT_MODE_MAILBOX_KHR` is currently not supported, because it needs special treatment.

#### dxvk.lowLatencyOffset

You can fine-tune the low-latency pacing options towards more fps or towards better latency. `dxvk.lowLatencyOffset = 0` is the default, a negative value will make frames start earlier by the given amount (in microseconds), and thus those frames will more likely run into buffering, which in turns may increase fps. A positive value will make frames start later by the given amount (in microseconds), which make it less likely to run into buffering and thus may improve latency.

In other words, this option has an effect on the percentage of frames which go into GPU buffering and/or v-sync buffering. A value of zero will make 50% of frames go (mostly slightly) into buffering, since for most games, the prediction is so accurate that it will average out to 0 microseconds.

For 360 fps gameplay, you may want to experiment with values in the range of -100 to 100. For less fps, you may want to use larger values respectively.

The offset is applied after predictions have been made to align the frame, but doesn't affect fps limiting.

#### dxvk.lowLatencyAllowCpuFramesOverlap

In case a game is generating a very high load (or specific load) on dxvk's CS thread (see `DXVK_HUD=cs`), setting `dxvk.lowLatencyAllowCpuFramesOverlap = False` will prevent the CS thread queue to create additional latency. 

Can be set to `False` on a game by game basis. By default, this option is set to `True`, because setting it to `False` can lead to certain type of stutters being magnified, for example from shader compiling, which can lead to strong fps loss in those cases.

### Additional considerations

#### Frame pipelining

Generating one frame of a video game typically can be seen as a sequence of the following operations:

`input sampling -> frame simulation -> frame rendering -> present()`

This frame pacing relies on games/engines following the simple principle of starting to process a new frame after finishing the previous one by calling the present() method of D3D. In the author's opinion, most games are actually doing this (at least for mouse input), as this is baked into the Direct 3D methodology, and games which care for input lag basically don't have another option. 

However, this is not guaranteed. There may be games out there not following this principle. For those, perfect frame pacing is impossible to achieve in the D3D domain.

#### Display Manager Presentation

When dxvk is finished rendering a frame, the resulting image still needs to be transported to the display which is relevant for latency too. 

Since there is not that much information available on this topic, the author has tested some configurations which may help you to make an informed decision. The following results were obtained running a game at 1800 fps, which has the option to flash a monitor region instantly during a mouse button press, such that Nvidia's Reflex Analyzer can be used to measure button-to-pixel latency on an Nvidia GPU (575.57.08 driver). These results may or may not be applicable to other hardware/drivers like AMD or Intel GPUs. 

The top of the screen was selected as monitor region to make VRR, Mailbox and V-Sync more comparable to tearing (Immediate) in terms of latency. Since the measured latency will depend on the state of the screen refresh when the mouse button was pressed, many such samples give a latency range. Note that Wine Wayland is still experimental (June 2025):

| Presentation      | Latency | fps |
| :---------------- | :------: | :----: |
| x11 flip immediate      |   1.0-4.1 ms   | 1700 fps |
| x11 flip v-sync         |   5.9-8.8 ms   | 360 fps |
|  |  |  |
| Xwayland mailbox KDE   |  4.5-7.3 ms     | 1800 fps |
| Xwayland v-sync KDE    |  11.9-14.7 ms   | 360 fps |
| Wine Wayland mailbox KDE   |  6.4-9.1 ms     | 360 fps | ## muss ich rechecken
| Wine Wayland v-sync KDE    |  5.5-8.3 ms  | 360 fps |
|  |  |  |
| Xwayland mailbox Gnome    |  3.7-6.4 ms  | 1800 fps |
| Xwayland v-sync Gnome    |  12.1-14.9 ms  | 360 fps |
| Wine Wayland mailbox Gnome    |  7.3-10.8 ms  | 1400 fps |
| Wine Wayland v-sync Gnome    |  5.9-8.7 ms  | 360 fps |
|  |  |  |
| x11 flip v-sync VRR | 1.4-4.4 ms | 357 fps |
| Wine Wayland KDE VRR| 1.7-4.6 ms | 357 fps |
| Wine Wayland Gnome VRR| 2.0-4.9 ms | 357 fps |

If you want to use x11, be sure that flip is enabled. This only works on single monitor configurations. On Nvidia, you can check if flip is enabled with `__GL_SHOW_GRAPHICS_OSD=1`. Gnome has had trouble to activate flip with Proton 10, but should work fine with Proton 9. KDE should enable flip pretty straight forward, as does startx and possibly other lightweight window managers.

#### Wiki

There are more things relevant to latency. SMT/Hyperthreading for example may increase latency by about 10% and may increase frame time variance depending on the game. SMT doesn't need to get disabled in BIOS, it can be turned on and off via command line too:

`sudo sh -c "echo off > /sys/devices/system/cpu/smt/control"`

The kernel/scheduler also plays a role for latency.

There will be a wiki section soon, discussing all these latency topics not directly related to dxvk.

#### Original behaviour

When `dxvk.framePace = "max-frame-latency"` and `dxvk.latencySleep = Auto` are set, dxvk low-latency will behave exactly how upstream dxvk does.



# DXVK (Original Description)

A Vulkan-based translation layer for Direct3D 8/9/10/11 which allows running 3D applications on Linux using Wine.

For the current status of the project, please refer to the [project wiki](https://github.com/doitsujin/dxvk/wiki).

The most recent development builds can be found [here](https://github.com/doitsujin/dxvk/actions/workflows/artifacts.yml?query=branch%3Amaster).

Release builds can be found [here](https://github.com/doitsujin/dxvk/releases).

## How to use
In order to install a DXVK package obtained from the [release](https://github.com/doitsujin/dxvk/releases) page into a given wine prefix, copy or symlink the DLLs into the following directories as follows, then open `winecfg` and manually add `native` DLL overrides for `d3d8`, `d3d9`, `d3d10core`, `d3d11` and `dxgi` under the Libraries tab.

In a default Wine prefix that would be as follows:
```
export WINEPREFIX=/path/to/wineprefix
cp x64/*.dll $WINEPREFIX/drive_c/windows/system32
cp x32/*.dll $WINEPREFIX/drive_c/windows/syswow64
winecfg
```

For a pure 32-bit Wine prefix (non default) the 32-bit DLLs instead go to the `system32` directory:
```
export WINEPREFIX=/path/to/wineprefix
cp x32/*.dll $WINEPREFIX/drive_c/windows/system32
winecfg
```

Verify that your application uses DXVK instead of wined3d by enabling the HUD (see notes below).

In order to remove DXVK from a prefix, remove the DLLs and DLL overrides, and run `wineboot -u` to restore the original DLL files.

Tools such as Steam Play, Lutris, Bottles, Heroic Launcher, etc will automatically handle setup of dxvk on their own when enabled.

#### DLL dependencies 
Listed below are the DLL requirements for using DXVK with any single API.

- d3d8: `d3d8.dll` and `d3d9.dll`
- d3d9: `d3d9.dll`
- d3d10: `d3d10core.dll`, `d3d11.dll` and `dxgi.dll`
- d3d11: `d3d11.dll` and `dxgi.dll`

### Notes on Vulkan drivers
Before reporting an issue, please check the [Wiki](https://github.com/doitsujin/dxvk/wiki/Driver-support) page on the current driver status and make sure you run a recent enough driver version for your hardware.

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
- `descriptors`: Shows the number of descriptor pools and descriptor sets.
- `memory`: Shows the amount of device memory allocated and used.
- `allocations`: Shows detailed memory chunk suballocation info.
- `gpuload`: Shows estimated GPU load. May be inaccurate.
- `version`: Shows DXVK version.
- `api`: Shows the D3D feature level used by the application.
- `cs`: Shows worker thread statistics.
- `compiler`: Shows shader compiler activity
- `samplers`: Shows the current number of sampler pairs used *[D3D9 Only]*
- `ffshaders`: Shows the current number of shaders generated from fixed function state *[D3D9 Only]*
- `swvp`: Shows whether or not the device is running in software vertex processing mode *[D3D9 Only]*
- `scale=x`: Scales the HUD by a factor of `x` (e.g. `1.5`)
- `opacity=y`: Adjusts the HUD opacity by a factor of `y` (e.g. `0.5`, `1.0` being fully opaque).

Additionally, `DXVK_HUD=1` has the same effect as `DXVK_HUD=devinfo,fps`, and `DXVK_HUD=full` enables all available HUD elements.

### Logs
When used with Wine, DXVK will print log messages to `stderr`. Additionally, standalone log files can optionally be generated by setting the `DXVK_LOG_PATH` variable, where log files in the given directory will be called `app_d3d11.log`, `app_dxgi.log` etc., where `app` is the name of the game executable.

On Windows, log files will be created in the game's working directory by default, which is usually next to the game executable.

### Frame rate limit
The `DXVK_FRAME_RATE` environment variable can be used to limit the frame rate. A value of `0` uncaps the frame rate, while any positive value will limit rendering to the given number of frames per second. Alternatively, the configuration file can be used.

### Device filter
Some applications do not provide a method to select a different GPU. In that case, DXVK can be forced to use a given device:
- `DXVK_FILTER_DEVICE_NAME="Device Name"` Selects devices with a matching Vulkan device name, which can be retrieved with tools such as `vulkaninfo`. Matches on substrings, so "VEGA" or "AMD RADV VEGA10" is supported if the full device name is "AMD RADV VEGA10 (LLVM 9.0.0)", for example. If the substring matches more than one device, the first device matched will be used.
- `DXVK_FILTER_DEVICE_UUID="00000000000000000000000000000001"` Selects a device by matching its Vulkan device UUID, which can also be retrieved using tools such as `vulkaninfo`. The UUID must be a 32-character hexadecimal string with no dashes. This method provides more precise selection, especially when using multiple identical GPUs.

**Note:** If the device filter is configured incorrectly, it may filter out all devices and applications will be unable to create a D3D device.

### Debugging
The following environment variables can be used for **debugging** purposes.
- `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` Enables Vulkan debug layers. Highly recommended for troubleshooting rendering issues and driver crashes. Requires the Vulkan SDK to be installed on the host system.
- `DXVK_LOG_LEVEL=none|error|warn|info|debug` Controls message logging.
- `DXVK_LOG_PATH=/some/directory` Changes path where log files are stored. Set to `none` to disable log file creation entirely, without disabling logging.
- `DXVK_DEBUG=markers|validation` Enables use of the `VK_EXT_debug_utils` extension for translating performance event markers, or to enable Vulkan validation, respecticely.
- `DXVK_CONFIG_FILE=/xxx/dxvk.conf` Sets path to the configuration file.
- `DXVK_CONFIG="dxgi.hideAmdGpu = True; dxgi.syncInterval = 0"` Can be used to set config variables through the environment instead of a configuration file using the same syntax. `;` is used as a seperator.

### Graphics Pipeline Library
On drivers which support `VK_EXT_graphics_pipeline_library` Vulkan shaders will be compiled at the time the game loads its D3D shaders, rather than at draw time. This reduces or eliminates shader compile stutter in many games when compared to the previous system.

In games that load their shaders during loading screens or in the menu, this can lead to prolonged periods of very high CPU utilization, especially on weaker CPUs. For affected games it is recommended to wait for shader compilation to finish before starting the game to avoid stutter and low performance. Shader compiler activity can be monitored with `DXVK_HUD=compiler`.

**Note:** Games which only load their D3D shaders at draw time (e.g. most Unreal Engine games) will still exhibit some stutter, although it should still be less severe than without this feature.

## Build instructions

In order to pull in all submodules that are needed for building, clone the repository using the following command:
```
git clone --recursive https://github.com/doitsujin/dxvk.git
```

### Requirements:
- [wine 7.1](https://www.winehq.org/) or newer
- [Meson](https://mesonbuild.com/) build system (at least version 0.58)
- [Mingw-w64](https://www.mingw-w64.org) compiler and headers (at least version 10.0)
- [glslang](https://github.com/KhronosGroup/glslang) compiler

### Building DLLs

#### The simple way
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
meson setup --cross-file build-win64.txt --buildtype release --prefix /your/dxvk/directory build.w64
cd build.w64
ninja install
```

The D3D8, D3D9, D3D10, D3D11 and DXGI DLLs will be located in `/your/dxvk/directory/bin`.

### Build troubleshooting
DXVK requires threading support from your mingw-w64 build environment. If you
are missing this, you may see "error: ‘std::cv_status’ has not been declared"
or similar threading related errors.

On Debian and Ubuntu, this can be resolved by using the posix alternate, which
supports threading. For example, choose the posix alternate from these
commands:
```
update-alternatives --config x86_64-w64-mingw32-gcc
update-alternatives --config x86_64-w64-mingw32-g++
update-alternatives --config i686-w64-mingw32-gcc
update-alternatives --config i686-w64-mingw32-g++
```
For non debian based distros, make sure that your mingw-w64-gcc cross compiler 
does have `--enable-threads=posix` enabled during configure. If your distro does
ship its mingw-w64-gcc binary with `--enable-threads=win32` you might have to
recompile locally or open a bug at your distro's bugtracker to ask for it. 

# DXVK Native

DXVK Native is a version of DXVK which allows it to be used natively without Wine.

This is primarily useful for game and application ports to either avoid having to write another rendering backend, or to help with port bringup during development.

[Release builds](https://github.com/doitsujin/dxvk/releases) are built using the Steam Runtime.

### How does it work?

DXVK Native replaces certain Windows-isms with a platform and framework-agnostic replacement, for example, `HWND`s can become `SDL_Window*`s, etc.
All it takes to do that is to add another WSI backend.

**Note:** DXVK Native requires a backend to be explicitly set via the `DXVK_WSI_DRIVER` environment variable. The current built-in options are `SDL3`, `SDL2`, and `GLFW`.

DXVK Native comes with a slim set of Windows header definitions required for D3D9/11 and the MinGW headers for D3D9/11.
In most cases, it will end up being plug and play with your renderer, but there may be certain teething issues such as:
- `__uuidof(type)` is supported, but `__uuidof(variable)` is not supported. Use `__uuidof_var(variable)` instead.
