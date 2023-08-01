d3d11_res = wrc_generator.process('version.rc')

dxgi_common_src = [
  '../dxgi/dxgi_format.cpp',
]

d3d10_src = [
  '../d3d10/d3d10_blend.cpp',
  '../d3d10/d3d10_buffer.cpp',
  '../d3d10/d3d10_depth_stencil.cpp',
  '../d3d10/d3d10_device.cpp',
  '../d3d10/d3d10_input_layout.cpp',
  '../d3d10/d3d10_multithread.cpp',
  '../d3d10/d3d10_query.cpp',
  '../d3d10/d3d10_rasterizer.cpp',
  '../d3d10/d3d10_sampler.cpp',
  '../d3d10/d3d10_texture.cpp',
  '../d3d10/d3d10_util.cpp',
  '../d3d10/d3d10_view_dsv.cpp',
  '../d3d10/d3d10_view_rtv.cpp',
  '../d3d10/d3d10_view_srv.cpp',
]

d3d11_src = [
  'd3d11_annotation.cpp',
  'd3d11_blend.cpp',
  'd3d11_buffer.cpp',
  'd3d11_class_linkage.cpp',
  'd3d11_cmdlist.cpp',
  'd3d11_context.cpp',
  'd3d11_context_def.cpp',
  'd3d11_context_ext.cpp',
  'd3d11_context_imm.cpp',
  'd3d11_cuda.cpp',
  'd3d11_depth_stencil.cpp',
  'd3d11_device.cpp',
  'd3d11_enums.cpp',
  'd3d11_features.cpp',
  'd3d11_fence.cpp',
  'd3d11_gdi.cpp',
  'd3d11_initializer.cpp',
  'd3d11_input_layout.cpp',
  'd3d11_interop.cpp',
  'd3d11_main.cpp',
  'd3d11_on_12.cpp',
  'd3d11_options.cpp',
  'd3d11_query.cpp',
  'd3d11_rasterizer.cpp',
  'd3d11_resource.cpp',
  'd3d11_sampler.cpp',
  'd3d11_shader.cpp',
  'd3d11_state.cpp',
  'd3d11_state_object.cpp',
  'd3d11_swapchain.cpp',
  'd3d11_texture.cpp',
  'd3d11_util.cpp',
  'd3d11_video.cpp',
  'd3d11_view_dsv.cpp',
  'd3d11_view_rtv.cpp',
  'd3d11_view_srv.cpp',
  'd3d11_view_uav.cpp',
]

d3d11_shaders = files([
  'shaders/d3d11_video_blit_frag.frag',
  'shaders/d3d11_video_blit_vert.vert',
])

d3d11_ld_args      = []
d3d11_link_depends = []

if platform == 'windows'
  d3d11_dxgi_dep = lib_dxgi
else
  d3d11_ld_args      += [ '-Wl,--version-script', join_paths(meson.current_source_dir(), 'd3d11.sym') ]
  d3d11_link_depends += files('d3d11.sym')
  d3d11_dxgi_dep = dxgi_dep
endif

d3d11_dll = shared_library('d3d11'+dll_ext, dxgi_common_src + d3d11_src + d3d10_src,
    glsl_generator.process(d3d11_shaders), d3d11_res,
  name_prefix         : dxvk_name_prefix,
  dependencies        : [ d3d11_dxgi_dep, dxbc_dep, dxvk_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  vs_module_defs      : 'd3d11'+def_spec_ext,
  link_args           : d3d11_ld_args,
  link_depends        : [ d3d11_link_depends ],
)

d3d11_dep = declare_dependency(
  link_with           : [ d3d11_dll ],
  include_directories : [ dxvk_include_path ],
)
