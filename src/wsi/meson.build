wsi_win32_src = [
  'win32/wsi_mode_win32.cpp',
  'win32/wsi_monitor_win32.cpp',
  'win32/wsi_presenter_win32.cpp',
  'win32/wsi_window_win32.cpp',
]

if dxvk_wsi == 'win32'
  wsi_src  = wsi_win32_src
  wsi_deps = []
else
  error('Unknown wsi')
endif

wsi_lib = static_library('wsi', wsi_src,
  dependencies        : wsi_deps,
  override_options    : ['cpp_std='+dxvk_cpp_std],
  include_directories : [ dxvk_include_path ])

wsi_dep = declare_dependency(
  link_with           : [ wsi_lib ],
  include_directories : [ dxvk_include_path ])
