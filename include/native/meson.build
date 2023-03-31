install_subdir(
  'directx',
  install_dir: get_option('includedir') / 'dxvk',
  strip_directory: true,
)

install_subdir(
  'windows',
  install_dir: get_option('includedir') / 'dxvk',
  strip_directory: true,
)

install_headers(
  'wsi/native_wsi.h',
  'wsi/native_@0@.h'.format(dxvk_wsi),
  subdir: 'dxvk/wsi',
)
