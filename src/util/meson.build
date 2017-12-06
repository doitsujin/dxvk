util_src = files([
  'com/com_guid.cpp',
  'com/com_private_data.cpp',
  
  'log/log.cpp',
  'log/log_debug.cpp',
  
  'sha1/sha1.c',
  'sha1/sha1_util.cpp',
])

util_lib = static_library('util', util_src,
  include_directories : [ dxvk_include_path ])
