d3d9_res = wrc_generator.process('version.rc')

d3d9_shaders = files([
  'shaders/d3d9_convert_yuy2_uyvy.comp',
  'shaders/d3d9_convert_l6v5u5.comp',
  'shaders/d3d9_convert_x8l8v8u8.comp',
  'shaders/d3d9_convert_a2w10v10u10.comp',
  'shaders/d3d9_convert_nv12.comp',
  'shaders/d3d9_convert_yv12.comp'
])

d3d9_src = [
  'd3d9_main.cpp',
  'd3d9_interface.cpp',
  'd3d9_adapter.cpp',
  'd3d9_monitor.cpp',
  'd3d9_device.cpp',
  'd3d9_state.cpp',
  'd3d9_cursor.cpp',
  'd3d9_swapchain.cpp',
  'd3d9_format.cpp',
  'd3d9_common_texture.cpp',
  'd3d9_texture.cpp',
  'd3d9_surface.cpp',
  'd3d9_volume.cpp',
  'd3d9_common_buffer.cpp',
  'd3d9_buffer.cpp',
  'd3d9_shader.cpp',
  'd3d9_vertex_declaration.cpp',
  'd3d9_query.cpp',
  'd3d9_multithread.cpp',
  'd3d9_options.cpp',
  'd3d9_stateblock.cpp',
  'd3d9_sampler.cpp',
  'd3d9_util.cpp',
  'd3d9_initializer.cpp',
  'd3d9_fixed_function.cpp',
  'd3d9_names.cpp',
  'd3d9_swvp_emu.cpp',
  'd3d9_format_helpers.cpp',
  'd3d9_hud.cpp'
]

d3d9_dll = shared_library('d3d9'+dll_ext, d3d9_src, glsl_generator.process(d3d9_shaders), d3d9_res,
  name_prefix         : '',
  dependencies        : [ dxso_dep, dxvk_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  vs_module_defs      : 'd3d9'+def_spec_ext,
  override_options    : ['cpp_std='+dxvk_cpp_std])

d3d9_dep = declare_dependency(
  link_with           : [ d3d9_dll ],
  include_directories : [ dxvk_include_path ])
