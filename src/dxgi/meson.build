dxgi_res = wrc_generator.process('version.rc')

dxgi_src = [
  'dxgi_adapter.cpp',
  'dxgi_enums.cpp',
  'dxgi_factory.cpp',
  'dxgi_format.cpp',
  'dxgi_main.cpp',
  'dxgi_monitor.cpp',
  'dxgi_options.cpp',
  'dxgi_output.cpp',
  'dxgi_surface.cpp',
  'dxgi_swapchain.cpp',
]

dxgi_ld_args      = []
dxgi_link_depends = []

if platform != 'windows'
  dxgi_ld_args      += [ '-Wl,--version-script', join_paths(meson.current_source_dir(), 'dxgi.sym') ]
  dxgi_link_depends += files('dxgi.sym')
endif

dxgi_dll = shared_library('dxgi'+dll_ext, dxgi_src, dxgi_res,
  name_prefix         : dxvk_name_prefix,
  dependencies        : [ dxvk_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  vs_module_defs      : 'dxgi'+def_spec_ext,
  link_args           : dxgi_ld_args,
  link_depends        : [ dxgi_link_depends ],
)

dxgi_dep = declare_dependency(
  link_with           : [ dxgi_dll ],
  include_directories : [ dxvk_include_path ],
)
