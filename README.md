# D7VK

A Vulkan-based translation layer for Direct3D 7, 6, 5 and 3 which allows running 3D applications on Linux using Wine. It uses DXVK's D3D9 backend as well as Wine's DDraw implementation (or the Windows native DDraw) and acts as a proxy between the two, providing a minimal D3D7/6/5/3-on-D3D9 implementation.

> [!IMPORTANT]
> D3D retained-mode applications are NOT supported, since the project only aims to implement immediate-mode.

> [!NOTE]
> D3D7/6/5/3 support is also available in [DXVK-Sarek](https://github.com/pythonlover02/DXVK-Sarek), in case you are using older graphics hardware.

## FAQ

### Will D7VK work with every game out there?

Sadly, no. DDraw and older D3D is a land of highly cursed API interoperability, and applications that for one reason or another mix and match D3D7/6/5/3/DDraw with GDI are generally not expected to work properly. In such cases, if games provide alternative renderers, based on Glide or OpenGL, I strongly recommend you use those, together with [nGlide](https://www.zeus-software.com/downloads/nglide) where applicable.

> [!TIP]
> If you're wondering about the current state of a certain game, a good starting point would be checking [the issue tracker](https://github.com/WinterSnowfall/d7vk/issues).

### Waiter, waiter! There's D3D6/5/3 support in my D7VK soup!

Well, yes, after looking over the D3D6/5 SDK documentation, they turned out to be somewhat approachable, so I have implemented both. Thanks to [CkNoSFeRaTU](https://github.com/CkNoSFeRaTU) we also support D3D3, along with execute buffer driven rendering, and thus have achieved full D3D (immediate-mode) coverage of the DDraw world.

### Why not spin off a D6VK, a D5VK and a D3VK, or rename the project?

All APIs prior to D3D8 fall under the cursed umbrella of DDraw, so it makes absolutely no sense to split things up. As for any renaming, that won't happen, since D3D7 support is still the main focus of the project.

### What happened to D3D1, D3D2 and D3D4?

- D3D1 never existed, because the first release of DirectX didn't include a 3D component at all.
- Direct3D (2) was added in DirectX 2 and was left mostly unchanged in DirectX 3, so D3D2 and D3D3 are interchangeable terms, with a purely historical distinction.
- [DirectX 4 was never released](https://devblogs.microsoft.com/oldnewthing/20040122-00/?p=40963), and the prototyped D3D4 implementation was ultimately restructured into what became D3D5.

### I don't have a Vulkan 1.4 capable GPU, so I can't use D7VK!

Then this is your lucky day, because through the efforts of [pythonlover02](https://github.com/pythonlover02), D7VK, aka D3D7/6/5/3 support for DXVK, was ported back to [DXVK-Sarek](https://github.com/pythonlover02/DXVK-Sarek). Apart from a few missing features (see below), you're getting everything D7VK has to offer, and you can now run it even on a Vulkan 1.1 capable GPU.

> [!WARNING]
> Support for the following D3D7/6/5/3 features is missing in the now ancient DXVK-Sarek backend:
> - Color key transparency
> - Dynamic (application-controlled) FSAA states
> - 8-bit R3G3B2 surfaces
> - Fixed function alternate pixel center handling

### Will DXVK's D3D9 config options, such as frame rate limits, work with D7VK?

Yes, because D7VK relies on DXVK's D3D9 backend, so everything ends up there anyway.

### VSync isn't turning off/on although the application lets me control it. What gives?

VSync is enabled by default with older D3D, and thus also in D7VK. In fact, older D3D devices have to explicitly expose support for being able to _turn off_ VSync, since not all of them were capable of doing it back in the day. Due to problematic implementations, given limited hardware support at the time, changing the default behavior may simply not work reliably, even if an option is provided.

> [!NOTE]
> D7VK properly supports disabling VSync if applications consistently perform flips using the `DDFLIP_NOVSYNC` flag, e.g. _Unreal Tournament_ with the OldUnreal patch applied, _Re-Volt_, _3DMark 2000_ and others.

That being said, D7VK will also enforce various frame rate limits, provided as built-in config options, for games that are known to break or suffer from various bugs at high frame rates. These situations are very much an issue on high refresh rate displays, regardless of VSync.

You can, however, use the traditional DXVK config options for controlling either frame rate limits or the presentation interval (VSync), namely: `d3d9.maxFrameRate` and `d3d9.presentInterval`, with values of your choosing, either to override any existing settings or to specify your own. Be warned that doing so is most likely going to cause issues, unless some form of mod/modern patch resolves the underlying physics/input handling/rendering limitations that many of these applications were confronted with at high frame rates.

### Is there a way to force enable AA?

Yes, use `ddraw.emulateFSAA = Forced`. Note that dynamic FSAA emulation is supported by D7VK, and some applications will outright provide you with the means to enable or disable it. Only use the above config option if you want to force enable AA, regardless of application support or set states. Please also keep in mind that force enabling AA may not work well in all cases, and screen edge artifacting and/or GUI element corruption are possible consequences.

Should you encounter any situation in which AA support is listed as unavailable or greyed out by an application (without it being forced, as per the above), please raise an issue on our tracker.

> [!WARNING]
> In [DXVK-Sarek](https://github.com/pythonlover02/DXVK-Sarek) you'll only be able to (optionally) force enable FSAA emulation, as the backend functionality required to let games dynamically control the AA state is missing.

### Will it work on Windows?

Maybe? I'm not using Windows, so can't test it or develop it to be adapted to such situations. Its primarily intended use case is, and always will be, Wine/Linux. To that end, D7VK is pretty much aligned with upstream DXVK.

### Will it be upstreamed to DXVK at some point?

No.

### Why not? Just do it!

Because DXVK's development team have made it clear they are not interested in merging and/or maintaining anything prior to D3D8. Also, considering this project takes a minimal approach in its DDraw implementation, essentially acting as a DDraw wrapper for D3D, it operates on a different principle compared to mainline DXVK, which is why it's best kept as a separate project altogether. I understand the desire to forge the One Ring, ehm... have things unified, but in this case it simply isn't meant to be.

## How to use

Grab the latest release or compile the project manually if you want to be "on the bleeding edge".

> [!WARNING]
> Please keep in mind that ABSOLUTELY NO TESTING is done on Windows. D7VK is developed on and primarily aimed at use with Wine/Linux, so your mileage may vary in other situations.

On Windows, simply copying D7VK's `ddraw.dll` next to the game executable usually works just fine, and is the only supported deployment option. If a game ships its own `ddraw.dll` file, you are expected to rename it (to something like `ddraw.dll.bak`) before bringing D7VK into the picture.

> [!CAUTION]
> Do NOT, I repeat, do NOT copy `ddraw.dll` in your system directory when using D7VK on Windows, since that may break your system, and you will need access to an actual DDraw implementation for D7VK to work.

On Linux, if you only want to give it a test in a Wine prefix of choice or only want to use it with a limited number of games, you can similarly copy D7VK's `ddraw.dll` file next to the game/application executable, then open `winecfg` and manually add `native, builtin` (explicitly in that order) DLL overrides for `ddraw` under the Libraries tab. Though this is expected to work in most cases, there are known exceptions of games which insist on loading dlls from the system path directly.

The more permanent and recommended deployment approach on Linux, which is in fact required by some games, such as _GTA 2_, _StarLancer_, _Midtown Madness 2_ and others, is to rename the system path Wine `ddraw.dll` file to `ddraw_.dll` and then copy D7VK's `ddraw.dll` in your `system32` (for a 32-bit prefix) or `syswow64` (for a 64-bit prefix) system path, in order to stand in its place. D7VK will prioritize loading the `ddraw_.dll` file from its current path before trying to load `ddraw.dll` from the system path, in order to accommodate both methods of use. You will need to set up proper dll overrides in Wine even in this case, as described above.

> [!CAUTION]
> Simply copying D7VK's `ddraw.dll` in your system path and replacing Wine's will NOT work, as D7VK doesn't implement DDraw, and as such it needs access to an actual DDraw implementation for any level of D3D to work.

> [!TIP]
> Verify that your application uses D7VK instead of WineD3D by enabling the HUD (see notes below).

#### DLL dependencies

Listed below are the DLL requirements for using D7VK with any single API.

- d3d7: `ddraw.dll`
- d3d6: `ddraw.dll`
- d3d5: `ddraw.dll`
- d3d3: `ddraw.dll`

> [!IMPORTANT]
> [DXVK-Sarek](https://github.com/pythonlover02/DXVK-Sarek) will also require the `d3d9.dll` file for each of the above listed APIs, in addition to `ddraw.dll`.

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
- `swvp`: Shows the vertex processing mode and the current number of software vertex processing shaders *[D3D9 Only]*
- `scale=x`: Scales the HUD by a factor of `x` (e.g. `1.5`)
- `opacity=y`: Adjusts the HUD opacity by a factor of `y` (e.g. `0.5`, `1.0` being fully opaque).

Additionally, `DXVK_HUD=1` has the same effect as `DXVK_HUD=devinfo,fps`, and `DXVK_HUD=full` enables all available HUD elements.

### Logs

When used with Wine, D7VK will print log messages to `stderr`. Additionally, standalone log files can optionally be generated by setting the `D7VK_LOG_PATH` variable, where log files in the given directory will be called `app_ddraw.log` etc., where `app` is the name of the game executable. The `D7VK_LOG_LEVEL` variable can be used to control logging verbosity.

The naming of the environment variables has been altered in order to allow for finer control of logging specifically for D7VK, independently of upstream DXVK.

> [!NOTE]
> D7VK v1.2 and earlier versions use the `DXVK_LOG_LEVEL` and `DXVK_LOG_PATH` variables, much like upstream DXVK.

On Windows, log files will be created in the game's working directory by default, which is usually next to the game executable.

## Any other doubts?

Please refer to the upstream DXVK wiki and documentation, available [here](https://github.com/doitsujin/dxvk).

Feel free to report any issues you encounter, should they not already be present on the issue tracker.

## Acknowledgments

None of this would have ever been possible without DXVK and Wine, so remember to show your love to the awesome people involved in those projects. A special thanks goes out to [AlpyneDreams](https://github.com/AlpyneDreams) (of D8VK fame), both for D8VK and also for providing a good reference experimental branch, without which I would not have even considered diving head first into spinning off D7VK.

An equally special thanks goes out to [CkNoSFeRaTU](https://github.com/CkNoSFeRaTU), who has helped the project with numerous contributions, such as color key transparency support, implementing D3D3 execute buffers, implementing legacy viewport transformations, adding a highly optimized CPU ProcessVertices implementation etc., and has done a metric ton of investigative work, which in turn led to countless bug fixes and many more games being supported by D7VK.

