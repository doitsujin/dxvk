dxvk_src = files([
  'dxvk_adapter.cpp',
  'dxvk_cmdlist.cpp',
  'dxvk_context.cpp',
  'dxvk_context_state.cpp',
  'dxvk_device.cpp',
  'dxvk_framebuffer.cpp',
  'dxvk_image.cpp',
  'dxvk_instance.cpp',
  'dxvk_lifetime.cpp',
  'dxvk_main.cpp',
  'dxvk_memory.cpp',
  'dxvk_renderpass.cpp',
  'dxvk_resource.cpp',
  'dxvk_surface.cpp',
  'dxvk_swapchain.cpp',
  'dxvk_sync.cpp',
  
  'vulkan/dxvk_vulkan_extensions.cpp',
  'vulkan/dxvk_vulkan_loader.cpp',
])

thread_dep = dependency('threads')

dxvk_lib = static_library('dxvk', dxvk_src,
  link_with           : [ util_lib ],
  dependencies        : [ thread_dep, lib_vulkan, lib_sdl2 ],
  include_directories : [ dxvk_include_path ])

dxvk_dep = declare_dependency(
  link_with           : [ dxvk_lib ],
  include_directories : [ dxvk_include_path, include_directories('.') ])