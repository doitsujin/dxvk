ddraw_res = wrc_generator.process('version.rc')

ddraw_src = [
  'ddraw_main.cpp',
  'ddraw_class_factory.cpp',
  'ddraw_common_interface.cpp',
  'ddraw_common_surface.cpp',
  'ddraw_clipper.cpp',
  'ddraw_gamma.cpp',
  'ddraw_options.cpp',
  'ddraw_palette.cpp',
  'ddraw/ddraw_interface.cpp',
  'ddraw/ddraw_surface.cpp',
  'ddraw2/ddraw2_interface.cpp',
  'ddraw2/ddraw2_surface.cpp',
  'ddraw2/ddraw3_surface.cpp',
  'ddraw4/ddraw4_interface.cpp',
  'ddraw4/ddraw4_surface.cpp',
  'ddraw7/ddraw7_interface.cpp',
  'ddraw7/ddraw7_surface.cpp',
  'd3d_common_device.cpp',
  'd3d_common_interface.cpp',
  'd3d_common_material.cpp',
  'd3d_common_texture.cpp',
  'd3d_common_viewport.cpp',
  'd3d_light.cpp',
  'd3d_multithread.cpp',
  'd3d3/d3d3_device.cpp',
  'd3d3/d3d3_execute_buffer.cpp',
  'd3d3/d3d3_interface.cpp',
  'd3d3/d3d3_material.cpp',
  'd3d3/d3d3_texture.cpp',
  'd3d3/d3d3_viewport.cpp',
  'd3d5/d3d5_device.cpp',
  'd3d5/d3d5_interface.cpp',
  'd3d5/d3d5_material.cpp',
  'd3d5/d3d5_texture.cpp',
  'd3d5/d3d5_viewport.cpp',
  'd3d6/d3d6_buffer.cpp',
  'd3d6/d3d6_device.cpp',
  'd3d6/d3d6_interface.cpp',
  'd3d6/d3d6_material.cpp',
  'd3d6/d3d6_viewport.cpp',
  'd3d7/d3d7_buffer.cpp',
  'd3d7/d3d7_interface.cpp',
  'd3d7/d3d7_device.cpp',
  'd3d7/d3d7_state_block.cpp',
]

ddraw_ld_args       = []
ddraw_link_depends  = []
ddraw_extra_dep     = []

if platform == 'windows'
  ddraw_extra_dep     += [ cpp.find_library('version') ]
else
  lib_d3d9 = d3d9_dep
  ddraw_ld_args      += [ '-Wl,--version-script', join_paths(meson.current_source_dir(), 'ddraw.sym') ]
  ddraw_link_depends += files('ddraw.sym')
endif

ddraw_dll = shared_library(dxvk_name_prefix+'ddraw', ddraw_src, ddraw_res,
  dependencies        : [ util_dep, dxvk_dep, ddraw_extra_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  vs_module_defs      : 'ddraw'+def_spec_ext,
  link_args           : ddraw_ld_args,
  link_depends        : [ ddraw_link_depends ],
  kwargs              : dxvk_so_version,
  link_with           : [ d3d9_static ],
)

ddraw_dep = declare_dependency(
  link_with           : [ ddraw_dll ],
  include_directories : [ dxvk_include_path ],
)

if platform != 'windows'
  pkg.generate(ddraw_dll,
    filebase: dxvk_pkg_prefix + 'ddraw',
    subdirs:  'dxvk',
  )
endif
