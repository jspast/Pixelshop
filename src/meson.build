blueprints = custom_target('blueprints',
  input: files(
    'gtk/help-overlay.blp',
    'pixelshop-window.blp',
  ),
  output: '.',
  command: [find_program('blueprint-compiler'), 'batch-compile', '@OUTPUT@', '@CURRENT_SOURCE_DIR@', '@INPUT@'],
)

pixelshop_sources = [
  'main.c',
  'pixelshop-application.c',
  'pixelshop-window.c',
]

pixelshop_deps = [
  dependency('gtk4'),
  dependency('libadwaita-1', version: '>= 1.4'),
  cc.find_library('m', required : false)
]

pixelshop_sources += gnome.compile_resources(
  'pixelshop-resources',
  'pixelshop.gresource.xml',
  dependencies: blueprints,
  c_name: 'pixelshop'
)

executable('pixelshop', pixelshop_sources,
  dependencies: pixelshop_deps,
       install: true,
)
