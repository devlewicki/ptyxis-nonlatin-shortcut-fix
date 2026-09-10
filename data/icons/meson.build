icondir = join_paths(get_option('datadir'), 'icons', 'hicolor')
symbolicdir = join_paths(icondir, 'symbolic', 'apps')
scalabledir = join_paths(icondir, 'scalable', 'apps')


if app_is_generic
  icon_prefix = 'generic'
else
  icon_prefix = 'ptyxis'
endif

if get_option('development')
  icon_prefix = '@0@-devel'.format(icon_prefix)
endif

install_data('@0@.svg'.format(icon_prefix), rename: '@0@.svg'.format(app_id), install_dir: scalabledir)
install_data('@0@-symbolic.svg'.format(icon_prefix), rename: '@0@-symbolic.svg'.format(app_id), install_dir: symbolicdir)
