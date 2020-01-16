d3d10core_debug = custom_target('d3d10core-debug',
  input : d3d10_core_dll,
  output : 'd3d10core.dll',
  command: dump_debug,
  install: true,
  install_dir: get_option('bindir')+'/.debug')

d3d10_debug = custom_target('d3d10-debug',
  input : d3d10_dll,
  output : 'd3d10.dll',
  command: dump_debug,
  install: true,
  install_dir: get_option('bindir')+'/.debug')

d3d10_1_debug = custom_target('d3d10_1-debug',
  input : d3d10_1_dll,
  output : 'd3d10_1.dll',
  command: dump_debug,
  install: true,
  install_dir: get_option('bindir')+'/.debug')

