---
name: macOS bug report
about: Report crashes, rendering issues, or WSI problems on native macOS builds
title: ''
labels: ''
assignees: ''

---

Please describe your issue as accurately as possible. Include screenshots or videos when relevant.

### Software information
- Application or game name:
- How you link/run SpockD3D9 (embedded dylib, custom loader, etc.):
- Relevant `dxvk.conf` settings:

### System information
- macOS version:
- Mac model / chip (Apple Silicon or Intel):
- GPU:
- WSI backend (`DXVK_WSI_DRIVER`): SDL2 / SDL3 / GLFW
- MoltenVK version (`brew info molten-vk` or bundled with app):
- SpockD3D9 build (commit or release tag):

### Steps to reproduce
1.
2.
3.

### Expected vs actual behavior


### Log files
Set before running:
```bash
export DXVK_LOG_LEVEL=debug
export DXVK_LOG_PATH=/tmp/dxvk-logs
mkdir -p /tmp/dxvk-logs
```

Attach the generated `*.log` files from `DXVK_LOG_PATH`.

Optional Vulkan validation:
```bash
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
```

### Additional context
- Fullscreen vs windowed:
- Single vs multi-monitor:
- Anything else that might help (apitrace, sample project, etc.)
