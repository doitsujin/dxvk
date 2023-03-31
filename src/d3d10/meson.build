d3d10_core_res = wrc_generator.process('version10_core.rc')

d3d10_core_src = [
  'd3d10_core.cpp',
]

d3d10_core_ld_args      = []
d3d10_core_link_depends = []

if platform == 'windows'
  d3d10_d3d11_dep = lib_d3d11
else
  d3d10_core_ld_args      += [ '-Wl,--version-script', join_paths(meson.current_source_dir(), 'd3d10core.sym') ]
  d3d10_core_link_depends += files('d3d10core.sym')
  d3d10_d3d11_dep = d3d11_dep
endif

d3d10_core_dll = shared_library(dxvk_name_prefix+'d3d10core', d3d10_core_src, d3d10_core_res,
  dependencies        : [ d3d10_d3d11_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  vs_module_defs      : 'd3d10core'+def_spec_ext,
  link_args           : d3d10_core_ld_args,
  link_depends        : [ d3d10_core_link_depends ],
  kwargs              : dxvk_so_version,
)

d3d10_core_dep = declare_dependency(
  link_with           : [ d3d10_core_dll ],
)

if platform != 'windows'
  pkg.generate(d3d10_core_dll,
    filebase: dxvk_pkg_prefix + 'd3d10core',
    subdirs:  'dxvk',
  )
endif
