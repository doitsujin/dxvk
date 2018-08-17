conf = configuration_data()
conf.set('dlldir', get_option('prefix')+'/'+get_option('bindir'))
conf.set('arch', target_machine.cpu_family())
conf.set('winelib', meson.get_cross_property('winelib', false))
configure_file(
  configuration : conf,
  input         : 'setup_dxvk.sh.in',
  output        : 'setup_dxvk.sh',
  install_dir   : get_option('bindir')
)
