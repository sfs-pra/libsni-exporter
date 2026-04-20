project('sni-exporter', ['c'],
  version: '0.2.0',
  license: 'MIT',
  default_options: ['warning_level=2']
)

subdir('src')

install_data(
  ['README.md', 'README.ru.md'],
  install_dir: join_paths(get_option('datadir'), 'doc', 'libsni-exporter')
)

install_data(
  ['examples/minimal-tray.c', 'examples/Makefile'],
  install_dir: join_paths(get_option('datadir'), 'doc', 'libsni-exporter')
)

install_data(
  ['vapi/sni-exporter.vapi', 'vapi/sni-exporter.deps'],
  install_dir: join_paths(get_option('datadir'), 'vala', 'vapi')
)

subdir('tests')
