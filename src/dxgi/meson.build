dxgi_src = [
  'dxgi_adapter.cpp',
  'dxgi_factory.cpp',
  'dxgi_main.cpp',
  'dxgi_output.cpp',
  'dxgi_private_data.cpp',
  'dxgi_swapchain.cpp',
]

dxgi_dll = shared_library('dxgi', dxgi_src,
  link_with           : [ util_lib ],
  dependencies        : [ dxvk_dep ],
  include_directories : dxvk_include_path)

dxgi_dep = declare_dependency(
  link_with           : [ dxgi_dll ],
  include_directories : [ dxvk_include_path, include_directories('.') ])
