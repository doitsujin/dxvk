# SpockD3D9 Compatibility Matrix

Native macOS D3D9 applications and ports using SpockD3D9 (`libdxvk_d3d9.dylib`). This table tracks known-good titles, broken cases, and suggested `dxvk.conf` profiles.

**Status legend**

| Status | Meaning |
|--------|---------|
| **Works** | Playable; no major rendering or stability issues |
| **Partial** | Runs but with visual glitches, performance issues, or missing features |
| **Broken** | Crashes, black screen, or unusable |
| **Untested** | Expected to work in theory; not yet verified on macOS |
| **N/A** | Windows/Wine binary only — use upstream DXVK + Wine, not SpockD3D9 |

Contributions welcome: test a title, add a row, and open a PR. For bugs use the [macOS bug report template](.github/ISSUE_TEMPLATE/bug_report_macos.md).

---

## Verified samples and tooling

| Title / sample | Status | WSI | Notes | `dxvk.conf` |
|----------------|--------|-----|-------|-------------|
| `d3d9-clear` (built-in smoke test) | **Works** | SDL2 | Clears back buffer and presents; exercised in CI | *(none)* |

---

## Reference DXVK Native ports (macOS untested)

These projects ship native builds using [DXVK Native](https://github.com/doitsujin/dxvk#dxvk-native) on Linux. They are strong candidates for a macOS port with SpockD3D9 but have **not** been verified here yet.

| Title | Status | Engine / API | WSI (typical) | Notes | Suggested starting profile |
|-------|--------|--------------|---------------|-------|---------------------------|
| [Perimeter](https://github.com/KranX/Perimeter) | **Untested** | Custom / D3D9 | SDL2 | Open-source RTS; DXVK Native on Linux | `dxvk.enableShaderCache = True` |
| [Momentum Mod](https://momentum-mod.org/) | **Untested** | Source / D3D9 | SDL2 | Source-engine mod; native Linux via DXVK Native | `dxvk.enableShaderCache = True` |
| Portal 2 / Left 4 Dead 2 (Valve) | **Untested** | Source / D3D9 | SDL2 | Valve shipped DXVK Native Linux builds; macOS would need a separate port | `dxvk.enableShaderCache = True` |
| Custom SDL2 D3D9 port (your project) | **Untested** | D3D9 | SDL2 / SDL3 / GLFW | Link `libdxvk_d3d9.dylib`, pass `SDL_Window*` as `HWND` | See [README](README.md#usage-in-your-application) |

---

## D3D9 port profiles (from upstream DXVK)

Configuration presets for **native ports** of titles that are known to need D3D9 quirks on Vulkan. Windows retail builds running under Wine are **N/A** for SpockD3D9 — use upstream DXVK instead.

| Title / pattern | Status (native macOS) | Typical issue | Suggested `dxvk.conf` |
|-----------------|----------------------|---------------|----------------------|
| The Sims 2 | **Untested** | Non-standard formats (X4R4G4B4), A8 RT misuse | `d3d9.supportX4R4G4B4 = True`, `d3d9.disableA8RT = True` |
| AquaNox / AquaNox 2 | **Untested** | Breaks when too many display modes are enumerated | `d3d9.modeCountCompatibility = True` |
| Halo: Combat Evolved | **Untested** | Wrong sampler/image type combinations | `d3d9.forceSamplerTypeSpecConstants = True` |
| Metal Gear Rising: Revengeance | **Untested** | Picks lowest refresh rate from mode list | `d3d9.forceRefreshRate = 60` |
| Silent Hill 2 (Enhanced Edition mod) | **Untested** | Single-buffer swapchain + front-buffer read | `d3d9.extraFrontbuffer = True` |
| Ultra-wide sensitive titles | **Untested** | Break on exotic aspect ratios | `d3d9.forceAspectRatio = "16:9"` |
| UE3 / UE4 D3D9 renderers | **Untested** | NVAPI / vendor detection paths | `d3d9.hideNvidiaGpu = Auto`, `dxvk.enableShaderCache = True` |
| Fixed-function heavy SM2 titles | **Untested** | Float emulation edge cases | `d3d9.floatEmulation = Strict` (if artifacts) |

---

## WSI backend notes (multi-monitor / fullscreen)

| Backend | Fullscreen on secondary monitor | Borderless desktop | Notes |
|---------|----------------------------------|--------------------|-------|
| **SDL3** | **Works** (recommended) | **Works** | Best-tested path; uses saved mode + closest display mode |
| **SDL2** | **Works** | **Works** | Centers on target display; parity with SDL3 |
| **GLFW** | **Partial** | **Partial** | Improved: deferred `setWindowMode`, closest video mode, monitor bounds; borderless uses undecorated window (not compositor FS) |

SpockD3D9 selects the target monitor from the window position (`getWindowMonitor`) when entering fullscreen, not always the primary display.

Test multi-monitor with:

```bash
export DXVK_WSI_DRIVER=SDL2   # or SDL3, GLFW
export DXVK_LOG_LEVEL=info
your_app
```

---

## Out of scope (Wine / Windows binaries)

| Category | Status | Notes |
|----------|--------|-------|
| Windows `.exe` via Wine + DXVK | **N/A** | Use [upstream DXVK](https://github.com/doitsujin/dxvk) or a Wine build |
| D3D10 / D3D11 titles | **N/A** | Disabled in default SpockD3D9 builds (`enable_d3d9` only) |
| Retail Steam D3D9 games (no source) | **N/A** | Require a native macOS port or wrapper that links SpockD3D9 |

---

## Common configuration snippets

### Apple Silicon — default tiler optimizations (recommended)

SpockD3D9 enables tiler-aware submission when MoltenVK is detected (`VK_DRIVER_ID_MOLTENVK`). Usually leave at default:

```ini
# dxvk.tilerMode = Auto
dxvk.enableShaderCache = True
```

### Stutter / present issues in fullscreen

```ini
# Prefer triple-buffering in app present params (BackBufferCount 2–3).
# If tearing is acceptable for testing:
# dxvk.tearFree = True
```

### Debug a failing title

```ini
# dxvk.enableDebugUtils = True
```

Run with:

```bash
export DXVK_LOG_LEVEL=debug
export DXVK_LOG_PATH=/tmp/dxvk-logs
mkdir -p /tmp/dxvk-logs
```

---

## What to include when reporting compatibility

1. Application name and how it links SpockD3D9
2. macOS version, chip (Apple Silicon / Intel), GPU
3. `DXVK_WSI_DRIVER` (SDL2 / SDL3 / GLFW)
4. MoltenVK version (`brew info molten-vk`)
5. Windowed vs fullscreen, single vs multi-monitor
6. Relevant `dxvk.conf` and `DXVK_LOG_LEVEL=debug` logs

See also [MoltenVK format and MSAA limits](docs/MOLTENVK_CAPABILITIES.md).
