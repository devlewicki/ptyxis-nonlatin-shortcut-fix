#include <gio/gio.h>

#include "ptyxis-podman-provider-private.h"

static void
test_podman_json (void)
{
  g_autoptr(PtyxisContainerProvider) provider = NULL;
  g_autoptr(GDir) dir = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *dirname = NULL;
  const char *name;

  dirname = g_build_filename (g_getenv ("G_TEST_SRCDIR"), "podman", NULL);
  g_assert_nonnull (dirname);

  provider = ptyxis_podman_provider_new ();
  g_assert_nonnull (provider);

  dir = g_dir_open (dirname, 0, &error);
  g_assert_no_error (error);
  g_assert_nonnull (dir);

  while ((name = g_dir_read_name (dir)))
    {
      g_autofree char *path = g_build_filename (dirname, name, NULL);
      g_autofree char *contents = NULL;
      gboolean r;
      gsize len;

      g_file_get_contents (path, &contents, &len, &error);
      g_assert_no_error (error);
      g_assert_nonnull (contents);

      g_debug ("Parsing `%s`", path);

      r = _ptyxis_podman_provider_parse_json (PTYXIS_PODMAN_PROVIDER (provider), contents, &error);
      g_assert_no_error (error);
      g_assert_true (r);
    }
}

int
main (int argc,
      char *argv[])
{
  if (g_getenv ("G_TEST_SRCDIR") == NULL)
    g_error ("G_TEST_SRCDIR must be set!");
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ptyxis/Podman/JSON", test_podman_json);
  return g_test_run ();
}
