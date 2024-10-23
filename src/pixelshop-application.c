#include "config.h"

#ifdef __unix__
#include <glib/gi18n.h>
#endif

#include "pixelshop-application.h"
#include "pixelshop-window.h"

struct _PixelshopApplication
{
	AdwApplication parent_instance;
};

G_DEFINE_FINAL_TYPE (PixelshopApplication, pixelshop_application, ADW_TYPE_APPLICATION)

PixelshopApplication *
pixelshop_application_new (const char        *application_id,
                           GApplicationFlags  flags)
{
	g_return_val_if_fail (application_id != NULL, NULL);

	return g_object_new (PIXELSHOP_TYPE_APPLICATION,
	                     "application-id", application_id,
	                     "flags", flags,
	                     NULL);
}

static void
pixelshop_application_activate (GApplication *app)
{
	GtkWindow *window;

	g_assert (PIXELSHOP_IS_APPLICATION (app));

	window = gtk_application_get_active_window (GTK_APPLICATION (app));

	if (window == NULL)
		window = g_object_new (PIXELSHOP_TYPE_WINDOW,
		                       "application", app,
		                       NULL);

	gtk_window_present (window);
}

static void
pixelshop_application_class_init (PixelshopApplicationClass *klass)
{
	GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

	app_class->activate = pixelshop_application_activate;
}

static void
pixelshop_application_about_action (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
	PixelshopApplication *self = user_data;
	GtkWindow *window = NULL;

	g_assert (PIXELSHOP_IS_APPLICATION (self));

	window = gtk_application_get_active_window (GTK_APPLICATION (self));

	adw_show_about_dialog (GTK_WIDGET (window),
	                       "application-name", "Pixelshop",
	                       "application-icon", "io.github.jspast.Pixelshop",
	                       "developer-name", "João S. Pastorello",
	                       "version", "0.1.0",
                         "issue-url", "https://github.com/jspast/Pixelshop/issues",
                         "website", "https://github.com/jspast/Pixelshop",
	                       NULL);
}

static void
pixelshop_application_quit_action (GSimpleAction *action,
                                   GVariant      *parameter,
                                   gpointer       user_data)
{
	PixelshopApplication *self = user_data;

	g_assert (PIXELSHOP_IS_APPLICATION (self));

	g_application_quit (G_APPLICATION (self));
}

static const GActionEntry app_actions[] = {
	{ "quit", pixelshop_application_quit_action },
	{ "about", pixelshop_application_about_action },
};

static void
pixelshop_application_init (PixelshopApplication *self)
{
	g_action_map_add_action_entries (G_ACTION_MAP (self),
	                                 app_actions,
	                                 G_N_ELEMENTS (app_actions),
	                                 self);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self),
	                                       "app.quit",
	                                       (const char *[]) { "<primary>q", NULL });

  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.open",
                                         (const char *[]) { "<Ctrl>o", NULL });

   gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.save-as",
                                         (const char *[]) {
                                           "<Ctrl><Shift>s",
                                           NULL,
                                         });
}

