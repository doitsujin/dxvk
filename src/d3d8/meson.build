d3d8_res = wrc_generator.process('version.rc')

d3d8_src = [
  'd3d8_buffer.cpp',
  'd3d8_device.cpp',
  'd3d8_interface.cpp',
  'd3d8_main.cpp',
  'd3d8_multithread.cpp',
  'd3d8_options.cpp',
  'd3d8_shader.cpp',
  'd3d8_state_block.cpp',
  'd3d8_surface.cpp',
  'd3d8_swapchain.cpp',
  'd3d8_texture.cpp',
  'd3d8_volume.cpp'
]

d3d8_ld_args      = []
d3d8_link_depends = []

if platform != 'windows'
  lib_d3d9 = d3d9_dep
  d3d8_ld_args      += [ '-Wl,--version-script', join_paths(meson.current_source_dir(), 'd3d8.sym') ]
  d3d8_link_depends += files('d3d8.sym')
endif

d3d8_dll = shared_library(dxvk_name_prefix+'d3d8', d3d8_src, d3d8_res,
  dependencies        : [ lib_d3d9, util_dep, dxso_dep, dxvk_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  vs_module_defs      : 'd3d8'+def_spec_ext,
  link_args           : d3d8_ld_args,
  link_depends        : [ d3d8_link_depends ],
  kwargs              : dxvk_so_version,
)

d3d8_dep = declare_dependency(
  link_with           : [ d3d8_dll ],
  include_directories : [ dxvk_include_path ],
)

if platform != 'windows'
  pkg.generate(d3d8_dll,
    filebase: dxvk_pkg_prefix + 'd3d8',
    subdirs:  'dxvk',
  )
endif
