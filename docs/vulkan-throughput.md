# DXVK Throughput-Optimized Build

This fork includes a throughput-optimized DXVK configuration, aimed at saturating Vulkan pipelines.  
Key changes:
- Max CPU threads for shader compilation
- Async pipeline & sparse residency
- Relaxed barriers for higher throughput
- Direct SSBO access
- Optimized GPU memory handling for 4GB devices

Use `dxvk-throughput.conf` to enable these settings.
