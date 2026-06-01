# Fallout 3 (Steam, Windows) — Compatibility Checklist

SpockD3D9 primary compatibility target. This document tracks every subsystem needed to get Fallout 3 running on macOS through SpockD3D9.

**Game details:**
- **Title:** Fallout 3 (+ DLCs / GOTY)
- **Platform:** Steam (Windows)
- **Engine:** Gamebryo (NetImmerse-derived)
- **Graphics API:** Direct3D 9 (SM2.0 / SM3.0, fixed-function fallbacks)
- **Resolution:** Up to 1920×1200 typically; higher with mods
- **Other APIs used:** DirectInput, DirectSound / XAudio2, Win32 filesystem, Win32 threading

---

## Execution model

Fallout 3 is a Windows `.exe`. To run it on macOS with SpockD3D9, a **wrapper or translation layer** must:

1. Load and execute the Windows binary (or a recompiled/patched version)
2. Intercept `d3d9.dll` imports and route them to `libdxvk_d3d9.dylib`
3. Translate Win32 windowing (HWND) to SDL2/SDL3/GLFW windows
4. Provide stubs or implementations for non-D3D9 APIs (DirectInput, DirectSound, filesystem)

**Options under consideration:**

| Approach | Pros | Cons |
|----------|------|------|
| Wine + SpockD3D9 as d3d9.dll override | Mature Win32 compat; proven approach | Upstream DXVK already works with Wine; duplicates effort |
| Custom lightweight wrapper | Minimal overhead; tailored to Gamebryo needs | Large surface area of Win32 APIs to implement |
| CrossOver / commercial Wine | Best macOS integration | License cost; may conflict with SpockD3D9 goals |
| Native source port (if available) | Best performance; cleanest integration | Fallout 3 source is not publicly available |

**Decision needed:** Choose the execution model before proceeding with integration work.

---

## D3D9 subsystem checklist

These are the D3D9 features Fallout 3 / Gamebryo is known to use. SpockD3D9 must handle all of them correctly.

### Device lifecycle

- [ ] `Direct3DCreate9` → `IDirect3D9` creation
- [ ] `IDirect3D9::CreateDevice` with appropriate flags
- [ ] `IDirect3DDevice9::TestCooperativeLevel` (focus loss / device lost)
- [ ] `IDirect3DDevice9::Reset` (resolution change, fullscreen toggle)
- [ ] Adapter enumeration (`GetAdapterCount`, `GetAdapterIdentifier`)
- [ ] Display mode enumeration (`EnumAdapterModes`, `GetAdapterDisplayMode`)
- [ ] `CheckDeviceFormat` for all formats Gamebryo queries
- [ ] `CheckDeviceMultiSampleType` (Gamebryo supports MSAA)
- [ ] `GetDeviceCaps` — verify SM3 caps, texture limits, render target caps

### Rendering

- [ ] Fixed-function vertex processing (Gamebryo uses FFP for some paths)
- [ ] Vertex shaders (SM2.0 and SM3.0)
- [ ] Pixel shaders (SM2.0 and SM3.0)
- [ ] Multiple render targets (MRT) — Gamebryo deferred lighting
- [ ] Alpha blending and alpha testing
- [ ] Stencil operations (shadow volumes, effects)
- [ ] Fog (vertex and table fog)
- [ ] Texture stage states (fixed-function multi-texturing)
- [ ] `DrawPrimitive` / `DrawIndexedPrimitive` (main draw paths)
- [ ] `DrawPrimitiveUP` / `DrawIndexedPrimitiveUP` (immediate-mode draws)
- [ ] Scissor test
- [ ] Viewport management

### Textures and formats

- [ ] DXT1 (BC1) — most world textures
- [ ] DXT3 (BC2) — some UI / alpha textures
- [ ] DXT5 (BC3) — normal maps, detail textures
- [ ] A8R8G8B8, X8R8G8B8 — render targets, UI
- [ ] R16F, R32F — HDR / float render targets (if Gamebryo HDR is enabled)
- [ ] D24S8 — primary depth/stencil
- [ ] D16 — shadow map depth (some configurations)
- [ ] L8, A8L8 — lightmaps, grayscale textures
- [ ] Volume textures (3D) — rare but possible in effects
- [ ] Cube maps — environment reflections
- [ ] Mipmapping and anisotropic filtering
- [ ] Texture addressing modes (wrap, clamp, mirror, border)

### Swap chain and presentation

- [ ] `Present` with various swap effects (`D3DSWAPEFFECT_DISCARD`, `_FLIP`, `_COPY`)
- [ ] Windowed mode presentation
- [ ] Exclusive fullscreen presentation
- [ ] `D3DPRESENT_INTERVAL_ONE` / `_IMMEDIATE` (vsync control)
- [ ] Back buffer format negotiation
- [ ] Triple buffering (`BackBufferCount = 2`)
- [ ] `GetFrontBufferData` (screenshots — used by some mods)
- [ ] `GetRenderTargetData` (render-to-texture readback)

### State management

- [ ] Render state block (`CreateStateBlock`, `BeginStateBlock`, `EndStateBlock`)
- [ ] All render states Gamebryo uses (see upstream DXVK for coverage)
- [ ] Sampler states (filtering, addressing, LOD bias, max anisotropy)
- [ ] Texture stage states
- [ ] Stream source management
- [ ] Vertex declaration

### Buffers

- [ ] Vertex buffers (MANAGED, DEFAULT, DYNAMIC pools)
- [ ] Index buffers (16-bit and 32-bit)
- [ ] `Lock` / `Unlock` with `DISCARD`, `NOOVERWRITE`, `READONLY` flags
- [ ] Proper MANAGED pool → device-local upload

### Queries

- [ ] Occlusion queries (`D3DQUERYTYPE_OCCLUSION`) — Gamebryo uses these
- [ ] Event queries (`D3DQUERYTYPE_EVENT`) — GPU fence / sync
- [ ] Timestamp queries (if used)

---

## Non-D3D9 dependencies

These are outside SpockD3D9's scope but must be provided by the wrapper layer for Fallout 3 to function.

| Subsystem | Windows API | Notes |
|-----------|-------------|-------|
| Input | DirectInput 8, Win32 messages (`WM_KEYDOWN`, etc.) | Keyboard, mouse, gamepad |
| Audio | DirectSound, XAudio2 | Music, SFX, voice |
| Filesystem | Win32 file APIs (`CreateFile`, `ReadFile`, etc.) | Saves, configs, BSA archives |
| Threading | Win32 threads, critical sections, events | Gamebryo is multi-threaded |
| Registry | `RegOpenKeyEx`, `RegQueryValueEx` | Steam game settings, install path |
| System info | `GetSystemInfo`, `GlobalMemoryStatusEx` | Hardware detection |
| Windowing | `CreateWindowEx`, `SetWindowPos`, message pump | Window creation and management |

---

## Suggested `dxvk.conf` profile

Starting configuration for Fallout 3 testing. Adjust based on test results.

```ini
# Fallout 3 (Steam) — SpockD3D9 profile
# Place next to FalloutLauncher.exe or set DXVK_CONFIG_FILE

# Shader cache — strongly recommended to reduce stutter
dxvk.enableShaderCache = True

# Tiler mode — leave at Auto for Apple Silicon
# dxvk.tilerMode = Auto

# Float emulation — try Strict if there are lighting artifacts
# d3d9.floatEmulation = Strict

# Force 60 Hz refresh rate (Gamebryo may pick wrong mode)
# d3d9.forceRefreshRate = 60

# Hide non-standard GPU vendor (avoid NVAPI/AMD-specific code paths)
# d3d9.hideNvidiaGpu = Auto
```

---

## Validation milestones

| Milestone | Description | Status |
|-----------|-------------|--------|
| **V1 — Library loads** | SpockD3D9 loads and `Direct3DCreate9` returns a valid object | Not started |
| **V2 — Device created** | `CreateDevice` succeeds with Gamebryo's requested parameters | Not started |
| **V3 — Boot to menu** | Fallout 3 main menu renders and is interactive | Not started |
| **V4 — New game loads** | Character creation / Vault 101 intro renders | Not started |
| **V5 — Outdoor rendering** | Capital Wasteland renders correctly (terrain, NPCs, sky) | Not started |
| **V6 — Interior rendering** | Indoor environments (Vault, buildings) render correctly | Not started |
| **V7 — Effects** | Particles, lighting, shadows, water render correctly | Not started |
| **V8 — Stability** | 30+ minutes of gameplay without crashes | Not started |
| **V9 — Save/load** | Save and load game works correctly | Not started |
| **V10 — Playable** | Full game is playable from start to finish | Not started |

---

## References

- [Fallout 3 on PCGamingWiki](https://www.pcgamingwiki.com/wiki/Fallout_3) — known issues, fixes, engine details
- [Gamebryo engine overview](https://en.wikipedia.org/wiki/Gamebryo) — engine architecture
- [DXVK Fallout 3 compatibility](https://github.com/doitsujin/dxvk/wiki) — upstream DXVK notes
- [MoltenVK capabilities](MOLTENVK_CAPABILITIES.md) — format and feature support on macOS
- [SpockD3D9 compatibility matrix](../COMPATIBILITY.md) — overall game tracker
