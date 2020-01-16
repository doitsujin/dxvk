d3d9_debug = custom_target('d3d9-debug',
  input : d3d9_dll,
  output : 'd3d9.dll',
  command: dump_debug,
  install: true,
  install_dir: get_option('bindir')+'/.debug')
