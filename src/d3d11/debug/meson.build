d3d11_debug = custom_target('d3d11-debug',
  input : d3d11_dll,
  output : 'd3d11.dll',
  command: dump_debug,
  install: true,
  install_dir: get_option('bindir')+'/.debug')
