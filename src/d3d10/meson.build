if dxvk_winelib
  d3d10_res = custom_target('version_10.res',
    input   : 'version10.rc',
    output  : 'version10.res',
    command : [ find_program('wrc'), '-o', '@OUTPUT@', '@INPUT@' ],
    install : false,
  )

  d3d10_1_res = custom_target('version_1_10.res',
    input   : 'version10_1.rc',
    output  : 'version10_1.res',
    command : [ find_program('wrc'), '-o', '@OUTPUT@', '@INPUT@' ],
    install : false,
  )

  d3d10_core_res = custom_target('version10_core.res',
    input   : 'version10_core.rc',
    output  : 'version10_core.res',
    command : [ find_program('wrc'), '-o', '@OUTPUT@', '@INPUT@' ],
    install : false,
  )
else
  d3d10_res = custom_target(
    'version10.res',
    input   : 'version10.rc',
    output  : 'version10.o',
    command : wrc,
  )

  d3d10_1_res = custom_target(
    'version10_1.res',
    input   : 'version10_1.rc',
    output  : 'version10_1.o',
    command : wrc,
  )

  d3d10_core_res = custom_target(
    'version10_core.res',
    input   : 'version10_core.rc',
    output  : 'version10_core.o',
    command : wrc,
  )
endif

d3d10_main_src = [
  'd3d10_main.cpp',
  'd3d10_reflection.cpp',
]

d3d10_deps = [ lib_d3dcompiler_43, lib_dxgi ]
d3d10_deps += dxvk_winelib ? lib_d3d11 : d3d11_dep

d3d10_core_dll = shared_library('d3d10core'+dll_ext, d3d10_main_src, not dxvk_winelib ? d3d10_core_res : [],
  name_prefix         : '',
  dependencies        : [ d3d10_deps, dxbc_dep, dxvk_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  objects             : not dxvk_msvc ? 'd3d10core'+def_spec_ext : [],
  vs_module_defs      : 'd3d10core'+def_spec_ext,
  override_options    : ['cpp_std='+dxvk_cpp_std])

d3d10_dll = shared_library('d3d10'+dll_ext, d3d10_main_src, not dxvk_winelib ? d3d10_res : [],
  name_prefix         : '',
  dependencies        : [ d3d10_deps, dxbc_dep, dxvk_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  objects             : not dxvk_msvc ? 'd3d10'+def_spec_ext : [],
  vs_module_defs      : 'd3d10'+def_spec_ext,
  override_options    : ['cpp_std='+dxvk_cpp_std])

d3d10_1_dll = shared_library('d3d10_1'+dll_ext, d3d10_main_src, not dxvk_winelib ? d3d10_1_res : [],
  name_prefix         : '',
  dependencies        : [ d3d10_deps, dxbc_dep, dxvk_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  objects             : not dxvk_msvc ? 'd3d10_1'+def_spec_ext : [],
  vs_module_defs      : 'd3d10_1'+def_spec_ext,
  override_options    : ['cpp_std='+dxvk_cpp_std])

if dxvk_winelib
  d3d10_dll_target = custom_target('d3d10.dll',
    output  : 'd3d10.dll',
    input   : [ 'd3d10.spec', d3d10_res ],
    command : [ find_program('winebuild'), target_arch, '--dll', '--fake-module', '-E', '@INPUT@', '-o', '@OUTPUT@', '-F', 'd3d10.dll' ],
    install : true,
    install_dir : 'fakedlls')

  d3d10_1_dll_target = custom_target('d3d10_1.dll',
    output  : 'd3d10_1.dll',
    input   : [ 'd3d10_1.spec', d3d10_1_res ],
    command : [ find_program('winebuild'), target_arch, '--dll', '--fake-module', '-E', '@INPUT@', '-o', '@OUTPUT@', '-F', 'd3d10_1.dll' ],
    install : true,
    install_dir : 'fakedlls')

  d3d10_core_dll_target = custom_target('d3d10core.dll',
    output  : 'd3d10core.dll',
    input   : [ 'd3d10core.spec', d3d10_core_res ],
    command : [ find_program('winebuild'), target_arch, '--dll', '--fake-module', '-E', '@INPUT@', '-o', '@OUTPUT@', '-F', 'd3d10core.dll' ],
    install : true,
    install_dir : 'fakedlls')
endif


d3d10_dep = declare_dependency(
  link_with           : [ d3d10_dll, d3d10_1_dll, d3d10_core_dll ],
  include_directories : [ dxvk_include_path ])
