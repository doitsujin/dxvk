dxgi_res = wrc_generator.process('version.rc')

dxgi_shaders = files([
  'shaders/dxgi_presenter_frag.frag',
  'shaders/dxgi_presenter_vert.vert',
])

dxgi_src = [
  'dxgi_adapter.cpp',
  'dxgi_enums.cpp',
  'dxgi_factory.cpp',
  'dxgi_format.cpp',
  'dxgi_main.cpp',
  'dxgi_monitor.cpp',
  'dxgi_options.cpp',
  'dxgi_output.cpp',
  'dxgi_swapchain.cpp',
]

dxgi_opts = common_shared_opts + { 'dependencies': [ dxvk_dep ], 'vs_module_defs': 'dxgi' + def_spec_ext }

if not dxvk_msvc
  dxgi_opts += { 'objects': 'dxgi' + def_spec_ext }
endif

dxgi_dll = shared_library('dxgi'+dll_ext, dxgi_src, dxgi_res, kwargs: dxgi_opts)

dxgi_dep = declare_dependency(
  link_with           : [ dxgi_dll ],
  include_directories : [ dxvk_include_path ])
