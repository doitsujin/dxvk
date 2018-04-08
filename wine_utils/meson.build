conf = configuration_data()
conf.set('dlldir', get_option('prefix')+'/'+get_option('bindir'))
conf.set('arch', target_machine.cpu_family())
if target_machine.cpu_family()=='x86'
  configure_file(
    configuration : conf,
    input         : 'setup_dxvk_32.sh.in',
    output        : 'setup_dxvk.sh',
    install_dir   : get_option('bindir')
  )
else
  configure_file(
    configuration : conf,
    input         : 'setup_dxvk.sh.in',
    output        : 'setup_dxvk.sh',
    install_dir   : get_option('bindir')
  )
endif
