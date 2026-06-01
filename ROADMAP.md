# SpockD3D9 Roadmap

Native macOS port of Direct3D 9 — D3D9 API calls translated to Vulkan (MoltenVK → Metal). This document tracks what is done, what is in progress, and what remains for a production-ready native library.

## Goals

- Ship `libdxvk_d3d9.dylib` for Apple Silicon (arm64) and Intel Mac (x86_64)
- Support SDL2, SDL3, and GLFW for window/surface integration (no Wine)
- Default builds include **D3D9 only** (D3D8/10/11/DXGI disabled in `meson_options.txt`)
- Optimize for Apple tiler GPUs via MoltenVK detection and upstream tiler heuristics

## Architecture

```
┌─────────────┐
│  Your App   │  D3D9 API (native, SDL/GLFW window)
├─────────────┤
│  SpockD3D9  │  D3D9 → Vulkan
├─────────────┤
│  MoltenVK   │  Vulkan → Metal
├─────────────┤
│    Metal    │
└─────────────┘
```

## Status Overview

| Area | Status |
|------|--------|
| Meson native build (arm64 / x86_64) | Done |
| `package-native.sh` packaging | Done |
| Universal dylib (`lipo`) packaging | Done |
| GitHub Actions macOS matrix build | Done |
| SDL2 / SDL3 / GLFW WSI backends | Partial |
| MoltenVK loader (`libvulkan.dylib` / `libMoltenVK.dylib`) | Done |
| Tiler GPU hints (`VK_DRIVER_ID_MOLTENVK`) | Done (upstream) |
| Runtime smoke test / sample app | Not started |
| Game compatibility matrix | Not started |
| macOS EDID / HDR metadata | Partial (EDID read; HDR path uses it) |
| Native D3D9 cursor | Partial (SDL2/GLFW HW + software compositing) |
| `isOccluded` for present throttling | Done (SDL2/SDL3/GLFW focus tracking) |
| Window focus/resize → `NotifyWindowActivated` | Done (SDL/GLFW polling path) |
| SDL2 fullscreen parity with SDL3 | Done |
| `GetDeviceCaps` Vulkan-derived limits | Done (anisotropy, texture dims, MSAA honesty) |

---

## Completed

- [x] Fork focused on D3D9-only native builds (`enable_d3d9=true`, others default `false`)
- [x] macOS meson adjustments (no `--build-id`, no static libgcc on darwin, native headers)
- [x] WSI driver selection via `DXVK_WSI_DRIVER` and SDL2/SDL3/GLFW compile-time flags
- [x] Vulkan loader tries MoltenVK on macOS (`src/vulkan/vulkan_loader.cpp`)
- [x] CI workflow: build both architectures, upload artifacts (`.github/workflows/build-macos.yml`)
- [x] README: build instructions, configuration, debugging env vars
- [x] D3D9 Vulkan interop exports (`d3d9_interop.cpp`) for native apps that need Vk handles
- [x] `isOccluded` implemented for SDL2, SDL3, and GLFW (focus-loss with 100 ms hysteresis)
- [x] SDL2 `enterFullscreenMode` now uses the mode saved by `setWindowMode` (parity with SDL3)
- [x] GLFW `getDesktopDisplayMode` returns the largest available mode (native resolution)
- [x] Universal binary (`lipo`) via `./package-native.sh … --arch universal`
- [x] SDL/GLFW window focus polling → `NotifyWindowActivated` (device-loss-on-focus path)
- [x] `GetDeviceCaps` uses Vulkan-derived texture dims, anisotropy, and volume extent; removes false MSAA-toggle and wideLines-conditioned AA-lines cap

---

## Milestones

### Milestone A — Builds and presents a pixel

- [x] CI installs MoltenVK and verifies Vulkan loader is present
- [x] Minimal native sample: `d3d9-clear` (SDL2 clear + present)
- [x] GLFW `setWindowMode` height typo fix
- [x] GLFW / SDL2 fullscreen targets the requested monitor (not always primary)
- [x] GLFW `getWindowMonitor` uses window position / fullscreen monitor

### Milestone B — Playable windowed D3D9 app

- [x] Window resize/focus without Win32 `WM_*` messages (SDL/GLFW event path → `NotifyWindowActivated`)
- [x] Software/hardware cursor support (`d3d9_cursor_native`, SDL2/GLFW)
- [x] `GetDeviceCaps` / adapter caps aligned with queried Vulkan/MoltenVK features
- [ ] Document MoltenVK format limits (BCn, depth, MSAA) and known gaps

### Milestone C — Fullscreen and display correctness

- [x] `getMonitorEdid` on macOS (IOKit + CoreGraphics, SDL2/SDL3/GLFW WSI)
- [x] `saveWindowState` / `restoreWindowState` for native WSI (SDL2, SDL3, GLFW)
- [x] SDL2 parity with SDL3 fullscreen path (display bounds, closest mode)
- [x] `isOccluded` for present throttling

### Milestone D — Production hardening

- [ ] Game compatibility table (title → status → `dxvk.conf` profile)
- [x] Universal dylib via `lipo` in `package-native.sh`
- [ ] Performance notes for tiler mode (`dxvk.tilerMode` in `dxvk.conf`)
- [x] macOS-focused issue template (`.github/ISSUE_TEMPLATE/bug_report_macos.md`)

---

## High Priority

### 1. WSI correctness (displays and fullscreen)

| Task | Files |
|------|-------|
| Multi-monitor exclusive/borderless fullscreen | `src/wsi/sdl2/wsi_window_sdl2.cpp`, `src/wsi/glfw/wsi_window_glfw.cpp` |
| SDL soname from Meson instead of hardcoded dylib names | `src/wsi/*/wsi_platform_*.cpp` |

### 2. Runtime validation

| Task | Files |
|------|-------|
| CI smoke test with MoltenVK | `.github/workflows/build-macos.yml`, new `examples/` or `tests/` |

### 3. MoltenVK capability audit

| Task | Files |
|------|-------|
| Document BCn / depth / MSAA support vs MoltenVK release | `ROADMAP.md`, README |

### 4. Native cursor

| Task | Files |
|------|-------|
| Implement or delegate `SetCursor` / `ShowCursor` | `src/d3d9/d3d9_cursor.cpp` |

---

## Medium Priority

- **Window lifecycle**: hook SDL/GLFW resize and focus → swapchain extent invalidation (`d3d9_swapchain.cpp`, `d3d9_window.cpp`)
- **HDR / colorimetry**: depends on EDID path (`d3d9_swapchain.cpp` consumer of `getMonitorEdid`)
- **SDL3 as recommended backend**: SDL3 WSI has the most complete fullscreen implementation; align README/CI defaults when stable
- **macOS `dxvk.conf` profile**: annotate MoltenVK-relevant keys; de-emphasize DXGI/D3D11 options
- **Present stats / VBlank**: stubs in `d3d9_swapchain.cpp` (`GetPresentStats`, `WaitForVBlank`)

---

## Low Priority

- Patch APIs (`DrawRectPatch`, `DrawTriPatch`) — rare in D3D9 titles (`d3d9_device.cpp`)
- Unimplemented render states (wrap, tessellation) — `d3d9_device.cpp`
- Fixed-function SM3 edge cases — `d3d9_fixed_function_vert.vert`
- Optional D3D8 build (`-Denable_d3d8=true`) for legacy titles
- Palette / indexed texture paths — `d3d9_device.cpp`

---

## Out of Scope (default builds)

- Wine / Proton integration (use upstream DXVK for that)
- D3D9On12 (`d3d9_on_12.cpp` stubs)
- DXGI / D3D10 / D3D11 (source retained; disabled via meson options)
- Direct Metal translation (see [dxmt](https://github.com/3Shain/dxmt))

---

## Contributing

Pick an unchecked item above, open a PR against `master`, and update this file when the milestone or task is completed. For bugs, use the macOS bug report template when available, and include: macOS version, GPU, WSI driver (`DXVK_WSI_DRIVER`), MoltenVK/Vulkan version, and `DXVK_LOG_LEVEL=debug` logs.
