wsi_win32_src = [
  'win32/wsi_monitor_win32.cpp',
  'win32/wsi_window_win32.cpp',
]

wsi_sdl2_src = [
  'sdl2/wsi_monitor_sdl2.cpp',
  'sdl2/wsi_window_sdl2.cpp',
]

if dxvk_wsi == 'win32'
  wsi_src  = wsi_win32_src
  wsi_deps = []
elif dxvk_wsi == 'sdl2'
  wsi_src  = wsi_sdl2_src
  wsi_deps = [ lib_sdl2 ]
else
  error('Unknown wsi')
endif

wsi_lib = static_library('wsi', wsi_src,
  dependencies        : wsi_deps,
  include_directories : [ dxvk_include_path ])

wsi_dep = declare_dependency(
  link_with           : [ wsi_lib ])
