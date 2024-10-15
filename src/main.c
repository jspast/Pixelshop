
#include "config.h"

#include <glib/gi18n.h>

#include "pixelshop-application.h"

int
main (int   argc,
      char *argv[])
{
	g_autoptr(PixelshopApplication) app = NULL;
	int ret;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	app = pixelshop_application_new ("io.github.jspast.Pixelshop", G_APPLICATION_DEFAULT_FLAGS);
	ret = g_application_run (G_APPLICATION (app), argc, argv);

	return ret;
}
