dxvk_shaders = files([
  'shaders/dxvk_blit_frag_1d.frag',
  'shaders/dxvk_blit_frag_2d.frag',
  'shaders/dxvk_blit_frag_3d.frag',

  'shaders/dxvk_buffer_to_image_d.frag',
  'shaders/dxvk_buffer_to_image_ds_export.frag',
  'shaders/dxvk_buffer_to_image_s_discard.frag',

  'shaders/dxvk_clear_buffer_u.comp',
  'shaders/dxvk_clear_buffer_f.comp',
  'shaders/dxvk_clear_image1d_u.comp',
  'shaders/dxvk_clear_image1d_f.comp',
  'shaders/dxvk_clear_image1darr_u.comp',
  'shaders/dxvk_clear_image1darr_f.comp',
  'shaders/dxvk_clear_image2d_u.comp',
  'shaders/dxvk_clear_image2d_f.comp',
  'shaders/dxvk_clear_image2darr_u.comp',
  'shaders/dxvk_clear_image2darr_f.comp',
  'shaders/dxvk_clear_image3d_u.comp',
  'shaders/dxvk_clear_image3d_f.comp',

  'shaders/dxvk_copy_buffer_image.comp',
  'shaders/dxvk_copy_color_1d.frag',
  'shaders/dxvk_copy_color_2d.frag',
  'shaders/dxvk_copy_color_ms.frag',
  'shaders/dxvk_copy_depth_stencil_1d.frag',
  'shaders/dxvk_copy_depth_stencil_2d.frag',
  'shaders/dxvk_copy_depth_stencil_ms.frag',

  'shaders/dxvk_cursor_vert.vert',
  'shaders/dxvk_cursor_frag.frag',

  'shaders/dxvk_dummy_frag.frag',

  'shaders/dxvk_fullscreen_geom.geom',
  'shaders/dxvk_fullscreen_vert.vert',
  'shaders/dxvk_fullscreen_layer_vert.vert',

  'shaders/dxvk_image_to_buffer_ds.comp',
  'shaders/dxvk_image_to_buffer_f.comp',

  'shaders/dxvk_present_frag.frag',
  'shaders/dxvk_present_frag_blit.frag',
  'shaders/dxvk_present_frag_ms.frag',
  'shaders/dxvk_present_frag_ms_amd.frag',
  'shaders/dxvk_present_frag_ms_blit.frag',
  'shaders/dxvk_present_vert.vert',

  'shaders/dxvk_resolve_frag_d.frag',
  'shaders/dxvk_resolve_frag_ds.frag',
  'shaders/dxvk_resolve_frag_f.frag',
  'shaders/dxvk_resolve_frag_i.frag',
  'shaders/dxvk_resolve_frag_u.frag',

  'hud/shaders/hud_chunk_frag_background.frag',
  'hud/shaders/hud_chunk_frag_visualize.frag',
  'hud/shaders/hud_chunk_vert_background.vert',
  'hud/shaders/hud_chunk_vert_visualize.vert',

  'hud/shaders/hud_frame_time_eval.comp',

  'hud/shaders/hud_graph_frag.frag',
  'hud/shaders/hud_graph_vert.vert',

  'hud/shaders/hud_text_frag.frag',
  'hud/shaders/hud_text_vert.vert',
])

dxvk_src = [
  'dxvk_access.cpp',
  'dxvk_adapter.cpp',
  'dxvk_allocator.cpp',
  'dxvk_barrier.cpp',
  'dxvk_buffer.cpp',
  'dxvk_cmdlist.cpp',
  'dxvk_compute.cpp',
  'dxvk_constant_state.cpp',
  'dxvk_context.cpp',
  'dxvk_cs.cpp',
  'dxvk_descriptor.cpp',
  'dxvk_device.cpp',
  'dxvk_device_filter.cpp',
  'dxvk_extensions.cpp',
  'dxvk_fence.cpp',
  'dxvk_format.cpp',
  'dxvk_framebuffer.cpp',
  'dxvk_gpu_event.cpp',
  'dxvk_gpu_query.cpp',
  'dxvk_graphics.cpp',
  'dxvk_image.cpp',
  'dxvk_implicit_resolve.cpp',
  'dxvk_instance.cpp',
  'dxvk_latency_builtin.cpp',
  'dxvk_latency_reflex.cpp',
  'dxvk_memory.cpp',
  'dxvk_meta_blit.cpp',
  'dxvk_meta_clear.cpp',
  'dxvk_meta_copy.cpp',
  'dxvk_meta_mipgen.cpp',
  'dxvk_meta_resolve.cpp',
  'dxvk_options.cpp',
  'dxvk_pipelayout.cpp',
  'dxvk_pipemanager.cpp',
  'dxvk_platform_exts.cpp',
  'dxvk_presenter.cpp',
  'dxvk_queue.cpp',
  'dxvk_sampler.cpp',
  'dxvk_shader.cpp',
  'dxvk_shader_key.cpp',
  'dxvk_signal.cpp',
  'dxvk_sparse.cpp',
  'dxvk_staging.cpp',
  'dxvk_state_cache.cpp',
  'dxvk_stats.cpp',
  'dxvk_swapchain_blitter.cpp',
  'dxvk_unbound.cpp',
  'dxvk_util.cpp',

  'hud/dxvk_hud.cpp',
  'hud/dxvk_hud_font.cpp',
  'hud/dxvk_hud_item.cpp',
  'hud/dxvk_hud_renderer.cpp',
]

if platform == 'windows'
  dxvk_src += [
    'dxvk_openvr.cpp',
    'dxvk_openxr.cpp',
  ]
endif

dxvk_extra_deps = [ dependency('threads') ]
if platform == 'linux'
  dxvk_extra_deps += [ cpp.find_library('dl', required: false) ]
endif

dxvk_lib = static_library('dxvk', dxvk_src, glsl_generator.process(dxvk_shaders), dxvk_version,
  link_with           : [ util_lib, spirv_lib, wsi_lib ],
  dependencies        : [ vkcommon_dep ] + dxvk_extra_deps,
  include_directories : [ dxvk_include_path ],
)

dxvk_dep = declare_dependency(
  link_with           : [ dxvk_lib ],
  include_directories : [ dxvk_include_path ],
)
