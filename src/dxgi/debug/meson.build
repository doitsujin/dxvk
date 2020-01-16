dxgi_debug = custom_target('dxgi-debug',
  input : dxgi_dll,
  output : 'dxgi.dll',
  command: dump_debug,
  install: true,
  install_dir: get_option('bindir')+'/.debug')
