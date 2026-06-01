# MoltenVK Capabilities on macOS

SpockD3D9 queries Vulkan (MoltenVK) at runtime for format and feature support. This document summarizes what D3D9 applications can expect on macOS, how SpockD3D9 reports caps, and where MoltenVK or Metal impose limits.

For the upstream MoltenVK feature list and known Vulkan gaps, see the [MoltenVK Runtime User Guide](https://github.com/KhronosGroup/MoltenVK/blob/master/Docs/MoltenVK_Runtime_UserGuide.md) and [supported Vulkan extensions](https://github.com/KhronosGroup/MoltenVK#supported-vulkan-features).

## How SpockD3D9 reports support

| D3D9 API | SpockD3D9 behavior |
|----------|-------------------|
| `CheckDeviceFormat` | Maps the D3D9 format to Vulkan, then queries `vkGetPhysicalDeviceFormatProperties` / image format properties for usage (texture, RT, depth, MSAA). |
| `CheckDeviceMultiSampleType` | Validates power-of-two sample counts against `framebufferColorSampleCounts` / `framebufferDepthSampleCounts`, with per-format fallback when needed. |
| `GetDeviceCaps` | Texture dimensions, anisotropy, volume extent, and AA line caps come from queried Vulkan limits (not hard-coded desktop values). |

If a format or usage is unsupported by MoltenVK/Metal, the corresponding D3D9 check returns `D3DERR_NOTAVAILABLE`.

---

## BCn block compression (DXT / BC)

D3D9 fourCC formats map to Vulkan BC block formats:

| D3D9 format | Vulkan format | Typical use |
|-------------|---------------|-------------|
| `D3DFMT_DXT1` | `VK_FORMAT_BC1_*` | Opaque / 1-bit alpha textures |
| `D3DFMT_DXT2`, `D3DFMT_DXT3` | `VK_FORMAT_BC2_*` | Explicit alpha |
| `D3DFMT_DXT4`, `D3DFMT_DXT5` | `VK_FORMAT_BC3_*` | Interpolated alpha |

**MoltenVK / Metal:** BC1–BC3 are supported for sampled textures on Apple Silicon and recent Intel Mac GPUs. Availability is reported through `CheckDeviceFormat`; do not assume support without checking.

**Known gaps:**

- **MSAA + BCn:** `CheckDeviceMultiSampleType` rejects multisampling on DXT surfaces (same rule as upstream DXVK). BC compressed targets cannot be MSAA render targets.
- **BC4 / BC5 / BC6H / BC7:** Not part of core D3D9; SpockD3D9 maps some extended formats where applicable, but most D3D9 titles only use DXT1–5.
- **PVRTC / ASTC:** MoltenVK exposes `VK_IMG_format_pvrtc` and ASTC on some Apple GPUs; D3D9 does not use these directly.

---

## Depth and stencil formats

Common D3D9 depth/stencil formats map to Vulkan depth/stencil images:

| D3D9 format | Notes |
|-------------|-------|
| `D3DFMT_D16` | Widely supported |
| `D3DFMT_D15S1` | Queried per adapter; may be unavailable |
| `D3DFMT_D24X8`, `D3DFMT_D24S8`, `D3DFMT_D24X4S4` | Depends on MoltenVK depth format support |
| `D3DFMT_D32`, `D3DFMT_D32F` | Often available as 32-bit depth |
| Lockable / vendor hacks (`D16_LOCKABLE`, `INTZ`, `RAWZ`, …) | Handled with format-specific rules; some are rejected for MSAA |

**MoltenVK / Metal:**

- Depth/stencil attachments generally work for standard 3D rendering.
- **`D3DFMT_D24S8` as a lockable system-memory surface** may not match Windows behavior; prefer `CheckDeviceFormat` before relying on lockable depth.
- **`D3DUSAGE_QUERY_DEPTHSTENCIL` / post-pixels shader depth** follow queried Vulkan format features.

**HDR / display metadata:** macOS EDID retrieval is implemented in the WSI layer; HDR output paths depend on display EDID and MoltenVK `VK_EXT_hdr_metadata` (macOS only).

---

## Multisample anti-aliasing (MSAA)

MSAA support is **format-dependent** and **power-of-two only** (2×, 4×, 8×, … up to what the device reports).

SpockD3D9 validates sample counts via:

1. `framebufferColorSampleCounts` / `framebufferDepthSampleCounts` from `VkPhysicalDeviceLimits`
2. Per-format optimal tiling queries when the general limit is insufficient

**Caps honesty:**

- `D3DPRASTERCAPS_MULTISAMPLE_TOGGLE` is **not** advertised. Vulkan does not allow toggling MSAA mid-render-pass the way some legacy D3D9 apps expect.
- MSAA is rejected for: lockable depth formats (`D16_LOCKABLE`, `D32F_LOCKABLE`, `D32_LOCKABLE`), `INTZ`, and all DXT formats.
- Maximum sample count varies by GPU and back-buffer format; use `CheckDeviceMultiSampleType` rather than assuming 8× is always valid.

**MoltenVK swapchain note:** Metal allows at most **three** concurrent swapchain images. Use triple-buffering (`BackBufferCount = 2` or `3` in D3D9 terms) for smoother present, especially in fullscreen.

---

## Other D3D9 ↔ MoltenVK gaps relevant to ports

| Area | Status / workaround |
|------|---------------------|
| Wide AA lines | Reported only when `wideLines` is enabled (common on Apple Silicon via MoltenVK). |
| 16K textures | `MaxTextureWidth/Height` follow `maxImageDimension2D` (typically 16 384 on Apple Silicon). |
| Anisotropy | Capped at min(driver limit, 16) in `GetDeviceCaps`. |
| Shader model 3 / fixed function | Translated to SPIR-V → MSL; rare edge cases may need title-specific `dxvk.conf` tweaks. |
| Portability enumeration | MoltenVK is a portability driver. When it advertises `VK_KHR_portability_enumeration`, SpockD3D9 enables that instance extension and sets `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR`, so the Khronos Vulkan loader reports the MoltenVK adapter instead of failing with "No adapters found". |

---

## Verifying support on your machine

Run with debug logging and exercise your app's format checks:

```bash
export DXVK_LOG_LEVEL=debug
export DXVK_WSI_DRIVER=SDL2   # or SDL3, GLFW
your_app
```

For a minimal end-to-end sanity check, build and run the included smoke test:

```bash
export DYLD_LIBRARY_PATH="/path/to/install/lib"
export DXVK_WSI_DRIVER=SDL2
/path/to/install/lib/d3d9-clear 3
```

MoltenVK version: `brew info molten-vk` or check the loader path printed in DXVK device info logs.

---

## References

- [MoltenVK supported Vulkan features](https://github.com/KhronosGroup/MoltenVK#supported-vulkan-features)
- [MoltenVK known limitations](https://github.com/KhronosGroup/MoltenVK/blob/master/Docs/MoltenVK_Runtime_UserGuide.md#known-moltenvk-limitations)
- SpockD3D9 format mapping: `src/d3d9/d3d9_format.cpp`
- Adapter queries: `src/d3d9/d3d9_adapter.cpp`
