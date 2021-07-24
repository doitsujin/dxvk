d3d8_res = wrc_generator.process('version.rc')

d3d8_src = [
  'd3d8_main.cpp',
  'd3d8_interface.cpp',
  'd3d8_device.cpp'
]

d3d8_dll = shared_library('d3d8'+dll_ext, d3d8_src, d3d8_res,
  name_prefix         : '',
  dependencies        : [ d3d9_dep, util_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  objects             : not dxvk_is_msvc ? 'd3d8'+def_spec_ext : [],
  vs_module_defs      : 'd3d8'+def_spec_ext,
  override_options    : ['cpp_std='+dxvk_cpp_std])

#d3d9_dep = declare_dependency(
#  link_with           : [ d3d9_dll ],
#  include_directories : [ dxvk_include_path ])
