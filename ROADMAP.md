# SpockD3D9 Roadmap

macOS D3D9 translation layer — D3D9 API calls translated to Vulkan (MoltenVK → Metal). This document tracks what is done, what is in progress, and what remains for production-ready game compatibility.

## Goals

- Ship `libdxvk_d3d9.dylib` for Apple Silicon (arm64) and Intel Mac (x86_64)
- Support SDL2, SDL3, and GLFW for window/surface integration
- Default builds include **D3D9 only** (D3D8/10/11/DXGI disabled in `meson_options.txt`)
- Optimize for Apple tiler GPUs via MoltenVK detection and upstream tiler heuristics
- **Achieve playable compatibility with Fallout 3 (Steam, Windows, Gamebryo/D3D9)** as the primary target title
- Close Win32 compatibility gaps needed for Windows game hosting

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
| SDL2 / SDL3 / GLFW WSI backends | Done (multi-monitor FS; GLFW borderless partial) |
| MoltenVK loader (`libvulkan.dylib` / `libMoltenVK.dylib`) | Done (auto-discovers Homebrew prefixes + ICD manifest) |
| Tiler GPU hints (`VK_DRIVER_ID_MOLTENVK`) | Done (upstream) |
| Runtime smoke test / sample app | Done (`d3d9-clear` SDL2 + SDL3, CI smoke step) |
| Game compatibility matrix | Partial (`COMPATIBILITY.md` — profiles + reference ports; needs macOS testing) |
| macOS EDID / HDR metadata | Partial (EDID read; HDR path uses it) |
| Native D3D9 cursor | Done (SDL2/SDL3/GLFW HW + software compositing) |
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
- [x] Vulkan loader auto-discovers Homebrew-installed MoltenVK (`/opt/homebrew`, `/usr/local`, `$HOMEBREW_PREFIX`) and points the loader at the MoltenVK ICD manifest when no `DYLD_LIBRARY_PATH` / `VK_ICD_FILENAMES` is set (`src/vulkan/vulkan_loader.cpp`)
- [x] Instance creation enables `VK_KHR_portability_enumeration` + `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR` when supported, so the Khronos Vulkan loader reports the MoltenVK portability driver instead of returning "No adapters found" (`src/dxvk/dxvk_instance.cpp`)
- [x] CI workflow: build both architectures, upload artifacts (`.github/workflows/build-macos.yml`)
- [x] README: build instructions, configuration, debugging env vars
- [x] D3D9 Vulkan interop exports (`d3d9_interop.cpp`) for native apps that need Vk handles
- [x] `isOccluded` implemented for SDL2, SDL3, and GLFW (focus-loss with 100 ms hysteresis)
- [x] SDL2 `enterFullscreenMode` now uses the mode saved by `setWindowMode` (parity with SDL3)
- [x] GLFW `getDesktopDisplayMode` returns the largest available mode (native resolution)
- [x] Universal binary (`lipo`) via `./package-native.sh … --arch universal`
- [x] SDL/GLFW window focus polling → `NotifyWindowActivated` (device-loss-on-focus path)
- [x] `GetDeviceCaps` uses Vulkan-derived texture dims, anisotropy, and volume extent; removes false MSAA-toggle and wideLines-conditioned AA-lines cap
- [x] MoltenVK format limits documented (`docs/MOLTENVK_CAPABILITIES.md`, README)
- [x] Game compatibility matrix scaffold (`COMPATIBILITY.md`)
- [x] Tiler mode performance notes in `dxvk.conf` and README
- [x] WSI library sonames resolved from Meson/pkg-config (`wsi_sonames.h`)
- [x] Multi-monitor fullscreen: D3D9 uses `getWindowMonitor`; GLFW/SDL3 WSI fixes (`d3d9_swapchain.cpp`, `wsi_window_glfw.cpp`, `wsi_window_sdl3.cpp`)
- [x] Compatibility matrix: DXVK Native reference ports and upstream D3D9 port profiles (`COMPATIBILITY.md`)

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
- [x] Software/hardware cursor support (`d3d9_cursor_native`, SDL2/SDL3/GLFW)
- [x] `GetDeviceCaps` / adapter caps aligned with queried Vulkan/MoltenVK features
- [x] Document MoltenVK format limits (BCn, depth, MSAA) and known gaps

### Milestone C — Fullscreen and display correctness

- [x] `getMonitorEdid` on macOS (IOKit + CoreGraphics, SDL2/SDL3/GLFW WSI)
- [x] `saveWindowState` / `restoreWindowState` for native WSI (SDL2, SDL3, GLFW)
- [x] SDL2 parity with SDL3 fullscreen path (display bounds, closest mode)
- [x] `isOccluded` for present throttling
- [x] Multi-monitor exclusive/borderless fullscreen (target monitor from window position)

### Milestone D — Production hardening

- [ ] Game compatibility table (title → status → `dxvk.conf` profile) — reference ports and upstream profiles in `COMPATIBILITY.md`; needs verified macOS runs
- [x] Universal dylib via `lipo` in `package-native.sh`
- [x] Performance notes for tiler mode (`dxvk.tilerMode` in `dxvk.conf`)
- [x] macOS-focused issue template (`.github/ISSUE_TEMPLATE/bug_report_macos.md`)

### Milestone E — Win32 compatibility shims

Close gaps in `src/util/util_win32_compat.h` and related native shims needed for Windows game compatibility.

| Task | Status | Priority | Notes |
|------|--------|----------|-------|
| `GetCurrentProcessId` / `GetCurrentProcess` | **Done** | High | `getpid()` / pseudo-handle `(HANDLE)-1` |
| `CloseHandle` | **Done** | High | Dispatches on `NativeHandleKind` tag; handles semaphore objects |
| `CreateSemaphoreA` / `ReleaseSemaphore` | **Done** | High | `dispatch_semaphore_t` on Apple; mutex+cv fallback on other unix |
| `SetEvent` | Stub (warns) | High | Event objects not yet implemented; no D3D9 code path calls this |
| `DuplicateHandle` | Stub (warns) | Medium | Not yet implemented; no active D3D9 code path calls this |
| `ProcessIdToSessionId` | **Done** | Low | Returns TRUE, session 0 (no Win32 sessions on macOS) |
| `CreateCompatibleDC` / `DeleteDC` | Stub (silent) | Low | GDI DC; Windows-only; returns nullptr/FALSE safely |

### Milestone F — Fallout 3 compatibility

Primary target: Fallout 3 (Steam, Windows) running on macOS via SpockD3D9. See [docs/FALLOUT3_COMPAT.md](docs/FALLOUT3_COMPAT.md) for the detailed checklist.

| Task | Status | Notes |
|------|--------|-------|
| Define execution model (wrapper / translation layer) | Not started | Decide how Windows binary is hosted on macOS |
| D3D9 device creation (Gamebryo) | Not started | Validate `Direct3DCreate9` → device → swapchain path |
| Shader compilation (SM2/SM3 + fixed-function) | Not started | Gamebryo uses mixed paths; test DXSO → SPIR-V → MSL chain |
| Texture format support (DXT1–5, depth) | Not started | Verify BCn + D24S8 on MoltenVK |
| Fullscreen / resolution enumeration | Not started | `EnumAdapterModes` → `Reset` cycle |
| Device lost / reset handling | Not started | Gamebryo calls `TestCooperativeLevel` + `Reset` on focus loss |
| `dxvk.conf` Fallout 3 profile | Not started | Title-specific quirk configuration |
| Boot-to-menu validation | Not started | First end-to-end milestone |
| In-game rendering validation | Not started | Outdoor + interior + NPC + effects |
| Save / load stability | Not started | Requires wrapper filesystem support |

---

## High Priority

### 1. WSI correctness (displays and fullscreen)

| Task | Files |
|------|-------|
| ~~Multi-monitor exclusive/borderless fullscreen~~ | Done — `d3d9_swapchain.cpp`, `wsi_window_glfw.cpp`, `wsi_window_sdl3.cpp`, `wsi_monitor_glfw.cpp` |
| ~~SDL soname from Meson instead of hardcoded dylib names~~ | Done — `src/wsi/wsi_sonames.h.in`, `src/wsi/meson.build` |

### 2. Runtime validation

| Task | Files |
|------|-------|
| ~~CI smoke test with MoltenVK~~ | Done — `.github/workflows/build-macos.yml`, `src/d3d9/examples/d3d9_clear.cpp` |

### 3. MoltenVK capability audit

| Task | Files |
|------|-------|
| ~~Document BCn / depth / MSAA support vs MoltenVK release~~ | Done — `docs/MOLTENVK_CAPABILITIES.md`, README |

### 4. Native cursor

| Task | Files |
|------|-------|
| ~~Implement or delegate `SetCursor` / `ShowCursor`~~ | Done — SDL2/SDL3/GLFW hardware + software cursor (`src/d3d9/d3d9_cursor.cpp`, `d3d9_cursor_native.cpp`) |

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

- D3D9On12 (`d3d9_on_12.cpp` stubs)
- DXGI / D3D10 / D3D11 (source retained; disabled via meson options)
- Direct Metal translation (see [dxmt](https://github.com/3Shain/dxmt))
- Non-D3D9 game APIs (DirectSound, DirectInput, XInput — needed for full game compatibility but outside SpockD3D9's responsibility; a wrapper layer must provide these)

---

## Contributing

Pick an unchecked item above, open a PR against `master`, and update this file when the milestone or task is completed. For bugs, use the macOS bug report template when available, and include: macOS version, GPU, WSI driver (`DXVK_WSI_DRIVER`), MoltenVK/Vulkan version, and `DXVK_LOG_LEVEL=debug` logs.
