gio_dep = dependency('gio-2.0')
pkg = import('pkgconfig')

srcs = [
  'sni-exporter.c',
]

libsni_exporter = shared_library('sni-exporter',
  srcs,
  dependencies: [gio_dep],
  version: meson.project_version(),
  soversion: '0',
  install: true,
)

install_headers('sni-exporter.h')

pkg.generate(
  libsni_exporter,
  filebase: 'sni-exporter',
  name: 'sni-exporter',
  description: 'Direct StatusNotifierItem and dbusmenu exporter library',
  subdirs: '.',
)
