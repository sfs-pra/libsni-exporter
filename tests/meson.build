gio_dep = dependency('gio-2.0')

test_menu = executable('test_menu',
  'test_menu.c',
  link_with: libsni_exporter,
  dependencies: [gio_dep],
  include_directories: include_directories('../src')
)

test('menu-properties', test_menu,
  timeout: 30
)
