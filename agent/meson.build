# We build the agent with older versions of GLib required as well
# as various linker symbols so that we can run the agent, unmodified
# on older enterprise linux distributions. Currently, ptyxis-agent
# can run on CentOS 7.

PTYXIS_AGENT_MIN_GLIB_MAJOR = 2
PTYXIS_AGENT_MIN_GLIB_MINOR = 56
PTYXIS_AGENT_MIN_GLIB_VERSION = '@0@.@1@'.format(PTYXIS_AGENT_MIN_GLIB_MAJOR, PTYXIS_AGENT_MIN_GLIB_MINOR)

ptyxis_agent_deps = [
  dependency('gio-2.0', version: '>= @0@'.format(PTYXIS_AGENT_MIN_GLIB_VERSION)),
  dependency('gio-unix-2.0', version: '>= @0@'.format(PTYXIS_AGENT_MIN_GLIB_VERSION)),
  dependency('json-glib-1.0'),
]

ptyxis_agent_ipc = gnome.gdbus_codegen('ptyxis-agent-ipc',
           sources: 'org.gnome.Ptyxis.Agent.xml',
  interface_prefix: 'org.gnome.Ptyxis.',
         namespace: 'PtyxisIpc',
)

ptyxis_agent_sources = [
  ptyxis_agent_ipc,
  files([
    'ptyxis-agent-util.c',
    'ptyxis-container-provider.c',
    'ptyxis-distrobox-container.c',
    'ptyxis-podman-container.c',
    'ptyxis-podman-provider.c',
    'ptyxis-process-impl.c',
    'ptyxis-run-context.c',
    'ptyxis-session-container.c',
    'ptyxis-toolbox-container.c',
    'ptyxis-unix-fd-map.c',
    'ptyxis-agent.c',
    'ptyxis-agent-impl.c',
  ]),
]

ptyxis_agent_link_args = []

ptyxis_agent_c_args = [
  release_link_args,
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_@0@_@1@'.format(PTYXIS_AGENT_MIN_GLIB_MAJOR, PTYXIS_AGENT_MIN_GLIB_MINOR),
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_@0@_@1@'.format(PTYXIS_AGENT_MIN_GLIB_MAJOR, PTYXIS_AGENT_MIN_GLIB_MINOR),
]

if get_option('libc-compat')
libc_compat = true
if target_machine.cpu_family() == 'x86_64'
  ptyxis_agent_c_args += ['-include', 'x86_64/force_link_glibc_2.17.h']
else
  libc_compat = false
endif

if libc_compat
  config_h.set10('LIBC_COMPAT', libc_compat)
  ptyxis_agent_link_args += ['-Wl,--wrap=__libc_start_main']
endif
endif

executable('ptyxis-agent', ptyxis_agent_sources,
         dependencies: ptyxis_agent_deps,
              install: true,
          install_dir: get_option('libexecdir'),
               c_args: ptyxis_agent_c_args,
            link_args: ptyxis_agent_link_args,
  include_directories: include_directories('..'),
)
