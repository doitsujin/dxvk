# SpockD3D9 Compatibility Matrix

Native macOS D3D9 applications and ports using SpockD3D9 (`libdxvk_d3d9.dylib`). This table tracks known-good titles, broken cases, and suggested `dxvk.conf` profiles.

**Status legend**

| Status | Meaning |
|--------|---------|
| **Works** | Playable; no major rendering or stability issues |
| **Partial** | Runs but with visual glitches, performance issues, or missing features |
| **Broken** | Crashes, black screen, or unusable |
| **Untested** | Expected to work in theory; not yet verified on macOS |
| **N/A** | Not a D3D9 target or out of scope (Windows/Wine binary) |

Contributions welcome: test a title, add a row, and open a PR. For bugs use the [macOS bug report template](.github/ISSUE_TEMPLATE/bug_report_macos.md).

---

## Verified samples and tooling

| Title / sample | Status | WSI | Notes | `dxvk.conf` |
|----------------|--------|-----|-------|-------------|
| `d3d9-clear` (built-in smoke test) | **Works** | SDL2 | Clears back buffer and presents; exercised in CI | *(none)* |

---

## Native D3D9 applications (community matrix)

| Title | Status | WSI | macOS / GPU tested | Notes | Suggested `dxvk.conf` |
|-------|--------|-----|-------------------|-------|----------------------|
| *(your app here)* | Untested | SDL2 | — | Link against `libdxvk_d3d9.dylib`; set `DXVK_WSI_DRIVER` | See [README](README.md#configuration) |

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
