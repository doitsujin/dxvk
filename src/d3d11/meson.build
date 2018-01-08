d3d11_src = [
  'd3d11_blend.cpp',
  'd3d11_buffer.cpp',
  'd3d11_class_linkage.cpp',
  'd3d11_context.cpp',
  'd3d11_depth_stencil.cpp',
  'd3d11_device.cpp',
  'd3d11_dummy_resource.cpp',
  'd3d11_enums.cpp',
  'd3d11_input_layout.cpp',
  'd3d11_main.cpp',
  'd3d11_present.cpp',
  'd3d11_query.cpp',
  'd3d11_rasterizer.cpp',
  'd3d11_sampler.cpp',
  'd3d11_shader.cpp',
  'd3d11_state.cpp',
  'd3d11_texture.cpp',
  'd3d11_util.cpp',
]

d3d11_dll = shared_library('d3d11', d3d11_src,
  name_prefix         : '',
  link_with           : [ util_lib ],
  dependencies        : [ dxvk_dep, dxgi_dep, dxbc_dep ],
  include_directories : dxvk_include_path,
  install             : true)

d3d11_dep = declare_dependency(
  link_with           : [ d3d11_dll ],
  include_directories : [ dxvk_include_path, include_directories('.') ])
