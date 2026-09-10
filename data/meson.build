if app_is_builder
  app_name = 'Builder Terminal'
elif app_is_generic
  app_name = 'Terminal'
else
  app_name = 'Ptyxis'
endif

desktop_config = configuration_data()
desktop_config.set('APP_ID', app_id)
desktop_config.set('APP_NAME', app_name)
desktop_file_in = configure_file(
          input: 'org.gnome.Ptyxis.desktop.in.in',
         output: 'org.gnome.Ptyxis.desktop.in',
  configuration: desktop_config,
)
desktop_file = i18n.merge_file(
        input: desktop_file_in,
       output: '@0@.desktop'.format(app_id),
         type: 'desktop',
       po_dir: '../po',
      install: true,
  install_dir: get_option('datadir') / 'applications'
)

desktop_utils = find_program('desktop-file-validate', required: false)
if desktop_utils.found()
  test('Validate desktop file', desktop_utils, args: [desktop_file])
endif

appstream_config = configuration_data()
appstream_config.set('APP_ID', app_id)
appstream_config.set('APP_NAME', app_name)
appstream_file_in = configure_file(
          input: 'org.gnome.Ptyxis.metainfo.xml.in.in',
         output: 'org.gnome.Ptyxis.metainfo.xml.in',
  configuration: appstream_config,
)
appstream_file = i18n.merge_file(
        input: appstream_file_in,
       output: '@0@.metainfo.xml'.format(app_id),
       po_dir: '../po',
      install: true,
  install_dir: join_paths(get_option('datadir'), 'metainfo'),
)


appstreamcli = find_program('appstreamcli', required: false, disabler: true)
test('Validate appstream file', appstreamcli,
     args: ['validate', '--no-net', '--explain', appstream_file])

compile_schemas = find_program('glib-compile-schemas', required: false, disabler: true)
test('Validate schema file',
     compile_schemas,
     args: ['--strict', '--dry-run', meson.current_source_dir()])

dbusconf = configuration_data()
dbusconf.set('app_id', app_id)
dbusconf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))
configure_file(
          input: 'org.gnome.Ptyxis.service.in',
         output: '@0@.service'.format(app_id),
  configuration: dbusconf,
        install: true,
    install_dir: join_paths(get_option('datadir'), 'dbus-1', 'services'),
)

subdir('icons')
