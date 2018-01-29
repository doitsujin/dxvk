conf = configuration_data()
conf.set('dlldir', get_option('prefix')+'/'+get_option('bindir'))
conf.set('arch', target_machine.cpu_family())
configure_file(
  configuration : conf,
  input         : 'dlls_setup.sh.in',
  output        : 'dlls_setup.sh',
  install_dir   : get_option('bindir')
)
