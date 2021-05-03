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
  'dxgi_swapchain.cpp',
]

dxgi_def = 'dxgi' + def_spec_ext

dxgi_opts = common_shared_opts + { 'dependencies': [ dxvk_dep ], 'vs_module_defs': dxgi_def }

if not dxvk_is_msvc
  dxgi_opts += { 'link_depends': files(dxgi_def), 'link_args': meson.current_source_dir() / dxgi_def }
endif

dxgi_dll = shared_library('dxgi'+dll_ext, dxgi_src, dxgi_res, kwargs: dxgi_opts)

dxgi_dep = declare_dependency(
  link_with           : [ dxgi_dll ],
  include_directories : [ dxvk_include_path ])
