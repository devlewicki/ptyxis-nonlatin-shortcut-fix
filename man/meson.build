man_config = configuration_data()
man_config.set('app_id', app_id)
man_page = configure_file(
          input: 'ptyxis.1.in',
         output: 'ptyxis.1',
  configuration: man_config,
)
install_man(man_page)
