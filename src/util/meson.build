util_src_files = [
  'com/com_guid.cpp',
  'com/com_private_data.cpp',

  'config/config.cpp',
  
  'log/log.cpp',
  'log/log_debug.cpp',
  
  'sha1/sha1.c',
  'sha1/sha1_util.cpp',
]

if dxvk_native
  util_src_files += 'util_env_native.cpp'
  util_src_files += 'thread_generic.cpp'
else
  util_src_files += 'util_env.cpp'
  util_src_files += 'util_string.cpp'
endif

util_src = files(util_src_files)

util_lib = static_library('util', util_src, com_headers,
  include_directories : [ dxvk_include_path ],
  override_options    : ['cpp_std='+dxvk_cpp_std])

util_dep = declare_dependency(
  link_with           : [ util_lib ])
