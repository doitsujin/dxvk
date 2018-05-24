dxgi_shaders = files([
  'shaders/dxgi_presenter_frag.frag',
  'shaders/dxgi_presenter_vert.vert',
])

dxgi_src = [
  'dxgi_adapter.cpp',
  'dxgi_device.cpp',
  'dxgi_enums.cpp',
  'dxgi_factory.cpp',
  'dxgi_format.cpp',
  'dxgi_main.cpp',
  'dxgi_options.cpp',
  'dxgi_output.cpp',
  'dxgi_presenter.cpp',
  'dxgi_swapchain.cpp',
]

dxgi_dll = shared_library('dxgi', dxgi_src, glsl_generator.process(dxgi_shaders),
  name_prefix         : '',
  link_with           : [ util_lib ],
  dependencies        : [ dxvk_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  vs_module_defs      : 'dxgi.def',
  override_options    : ['cpp_std='+dxvk_cpp_std])

dxgi_dep = declare_dependency(
  link_with           : [ dxgi_dll ],
  include_directories : [ dxvk_include_path, include_directories('.') ])
