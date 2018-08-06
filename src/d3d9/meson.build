d3d9_src = [
]

d3d9_dll = shared_library('d3d9'+dll_ext, d3d9_src,
  name_prefix         : '',
  link_with           : [ util_lib ],
  dependencies        : [ lib_d3d11, lib_dxgi, dxbc_dep, dxvk_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  objects             : not dxvk_msvc ? 'd3d9'+def_spec_ext : [],
  vs_module_defs      : 'd3d9'+def_spec_ext,
  override_options    : ['cpp_std='+dxvk_cpp_std])

d3d9_dep = declare_dependency(
  link_with           : [ d3d9_dll ],
  include_directories : [ dxvk_include_path, include_directories('.') ])
