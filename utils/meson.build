conf = configuration_data()
conf.set('bindir', get_option('bindir'))
conf.set('libdir', get_option('libdir'))
conf.set('arch', target_machine.cpu_family())
conf.set('winelib', dxvk_winelib)
configure_file(
  configuration : conf,
  input         : 'setup_dxvk.sh.in',
  output        : 'setup_dxvk.sh',
  install_dir   : get_option('bindir')
)
