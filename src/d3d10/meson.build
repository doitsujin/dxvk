d3d10_res      = wrc_generator.process('version10.rc')
d3d10_1_res    = wrc_generator.process('version10_1.rc')
d3d10_core_res = wrc_generator.process('version10_core.rc')

d3d10_main_src = [
  'd3d10_main.cpp',
  'd3d10_reflection.cpp',
]

d3d10_deps = [ lib_d3dcompiler_43, lib_dxgi ]
d3d10_deps += dxvk_winelib ? lib_d3d11 : d3d11_dep

d3d10_common_opts = common_shared_opts + { 'dependencies': [ d3d10_deps, dxbc_dep, dxvk_dep ] }

d3d10_core_opts = d3d10_common_opts + { 'vs_module_defs': 'd3d10core' + def_spec_ext }
d3d10_opts      = d3d10_common_opts + { 'vs_module_defs': 'd3d10'     + def_spec_ext }
d3d10_1_opts    = d3d10_common_opts + { 'vs_module_defs': 'd3d10_1'   + def_spec_ext }

if not dxvk_msvc
  d3d10_core_opts += { 'objects': 'd3d10core' + def_spec_ext }
  d3d10_opts      += { 'objects': 'd3d10'     + def_spec_ext }
  d3d10_1_opts    += { 'objects': 'd3d10_1'   + def_spec_ext }
endif

d3d10_core_dll = shared_library('d3d10core'+dll_ext, d3d10_main_src, d3d10_core_res, kwargs: d3d10_core_opts)
d3d10_dll      = shared_library('d3d10'+dll_ext,     d3d10_main_src, d3d10_res,      kwargs: d3d10_opts)
d3d10_1_dll    = shared_library('d3d10_1'+dll_ext,   d3d10_main_src, d3d10_1_res,    kwargs: d3d10_1_opts)

d3d10_dep = declare_dependency(
  link_with           : [ d3d10_dll, d3d10_1_dll, d3d10_core_dll ],
  include_directories : [ dxvk_include_path ])
