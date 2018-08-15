d3d10_main_src = [
  'd3d10_main.cpp',
  'd3d10_reflection.cpp',
]

d3d10_core_dll = shared_library('d3d10core'+dll_ext, d3d10_main_src,
  name_prefix         : '',
  link_with           : [ util_lib ],
  dependencies        : [ lib_d3dcompiler_43, lib_dxgi, d3d11_dep, dxbc_dep, dxvk_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  objects             : not dxvk_msvc ? 'd3d10core'+def_spec_ext : [],
  vs_module_defs      : 'd3d10core'+def_spec_ext,
  override_options    : ['cpp_std='+dxvk_cpp_std])

d3d10_dll = shared_library('d3d10'+dll_ext, d3d10_main_src,
  name_prefix         : '',
  link_with           : [ util_lib ],
  dependencies        : [ lib_d3dcompiler_43, lib_dxgi, d3d11_dep, dxbc_dep, dxvk_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  objects             : not dxvk_msvc ? 'd3d10'+def_spec_ext : [],
  vs_module_defs      : 'd3d10'+def_spec_ext,
  override_options    : ['cpp_std='+dxvk_cpp_std])

d3d10_1_dll = shared_library('d3d10_1'+dll_ext, d3d10_main_src,
  name_prefix         : '',
  link_with           : [ util_lib ],
  dependencies        : [ lib_d3dcompiler_43, lib_dxgi, d3d11_dep, dxbc_dep, dxvk_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  objects             : not dxvk_msvc ? 'd3d10_1'+def_spec_ext : [],
  vs_module_defs      : 'd3d10_1'+def_spec_ext,
  override_options    : ['cpp_std='+dxvk_cpp_std])

d3d10_dep = declare_dependency(
  link_with           : [ d3d10_dll, d3d10_1_dll, d3d10_core_dll ],
  include_directories : [ dxvk_include_path, include_directories('.') ])
