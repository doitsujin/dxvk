dxvk_hud_shaders = files([
  'hud/shaders/hud_text_frag.frag',
  'hud/shaders/hud_text_vert.vert',
])

dxvk_src = files([
  'dxvk_adapter.cpp',
  'dxvk_barrier.cpp',
  'dxvk_buffer.cpp',
  'dxvk_buffer_res.cpp',
  'dxvk_cmdlist.cpp',
  'dxvk_compute.cpp',
  'dxvk_context.cpp',
  'dxvk_cs.cpp',
  'dxvk_data.cpp',
  'dxvk_descriptor.cpp',
  'dxvk_device.cpp',
  'dxvk_extensions.cpp',
  'dxvk_event.cpp',
  'dxvk_event_tracker.cpp',
  'dxvk_format.cpp',
  'dxvk_framebuffer.cpp',
  'dxvk_graphics.cpp',
  'dxvk_image.cpp',
  'dxvk_instance.cpp',
  'dxvk_lifetime.cpp',
  'dxvk_main.cpp',
  'dxvk_memory.cpp',
  'dxvk_meta_resolve.cpp',
  'dxvk_options.cpp',
  'dxvk_pipecache.cpp',
  'dxvk_pipelayout.cpp',
  'dxvk_pipemanager.cpp',
  'dxvk_query.cpp',
  'dxvk_query_pool.cpp',
  'dxvk_query_tracker.cpp',
  'dxvk_queue.cpp',
  'dxvk_renderpass.cpp',
  'dxvk_resource.cpp',
  'dxvk_sampler.cpp',
  'dxvk_shader.cpp',
  'dxvk_staging.cpp',
  'dxvk_surface.cpp',
  'dxvk_swapchain.cpp',
  'dxvk_sync.cpp',
  'dxvk_unbound.cpp',
  'dxvk_util.cpp',
  
  'hud/dxvk_hud.cpp',
  'hud/dxvk_hud_devinfo.cpp',
  'hud/dxvk_hud_font.cpp',
  'hud/dxvk_hud_fps.cpp',
  'hud/dxvk_hud_text.cpp',
  
  'vulkan/dxvk_vulkan_extensions.cpp',
  'vulkan/dxvk_vulkan_loader.cpp',
  'vulkan/dxvk_vulkan_names.cpp',
])

thread_dep = dependency('threads')

dxvk_lib = static_library('dxvk', dxvk_src, glsl_generator.process(dxvk_hud_shaders),
  link_with           : [ util_lib, spirv_lib ],
  dependencies        : [ thread_dep, lib_vulkan ],
  include_directories : [ dxvk_include_path ],
  override_options    : ['cpp_std='+dxvk_cpp_std])

dxvk_dep = declare_dependency(
  link_with           : [ dxvk_lib ],
  include_directories : [ dxvk_include_path, include_directories('.') ])