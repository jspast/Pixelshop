#include "config.h"

#include "pixelshop-window.h"
#include "pixelshop-image.h"
#include "pixelshop-histogram-window.h"

struct _PixelshopWindow
{
  AdwApplicationWindow parent_instance;

  gboolean image_opened;

  AdwToolbarView *toolbar_view;

  GtkStack *stack;
  GtkRevealer *edit_button_revealer;
  GtkToggleButton *edit_button;
  AdwOverlaySplitView *split_view;

  AdwActionRow *hmirror_button;
  AdwActionRow *vmirror_button;
  AdwSwitchRow *grayscale_button;
  AdwSpinRow *quantization_button;

  GtkBox *pictures_box;
  GtkPicture *original_picture;
  GtkPicture *edited_picture;

  PixelshopImage *image;
};

G_DEFINE_FINAL_TYPE (PixelshopWindow, pixelshop_window, ADW_TYPE_APPLICATION_WINDOW)

static void
pixelshop_window_class_init (PixelshopWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/jspast/Pixelshop/pixelshop-window.ui");

  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, toolbar_view);

  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, stack);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, edit_button_revealer);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, edit_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, split_view);

  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, hmirror_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, vmirror_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, grayscale_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, quantization_button);

  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, pictures_box);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, original_picture);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, edited_picture);
}

static void
draw_original_picture (PixelshopWindow *self)
{
  gtk_picture_set_paintable (self->original_picture,
                             GDK_PAINTABLE(pixelshop_image_get_original_texture(self->image)));
}

static void
draw_edited_picture (PixelshopWindow *self)
{
  gtk_picture_set_paintable (self->edited_picture,
                             GDK_PAINTABLE(pixelshop_image_get_edited_texture(self->image)));
}

static void
on_mirror_horizontally (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
  pixelshop_image_toggle_mirrored_horizontally(self->image);

  draw_edited_picture (self);
}

static void
on_mirror_vertically (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
  pixelshop_image_toggle_mirrored_vertically(self->image);

  draw_edited_picture (self);
}

static void
toggle_grayscale (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
  pixelshop_image_toggle_grayscale(self->image);

  draw_edited_picture (self);
}

static void
on_colors_changed (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
  int colors = adw_spin_row_get_value(self->quantization_button);

  if (colors < 256){
    pixelshop_image_set_quantization_colors (colors, self->image);
    draw_edited_picture (self);
  }
}

static void
reset_buttons (PixelshopWindow *self)
{
  gtk_list_box_row_set_selectable((GtkListBoxRow*)self->grayscale_button, false);
  adw_spin_row_set_value(self->quantization_button, 256);
}

static void
stack_transition (PixelshopWindow *self)
{
  gtk_stack_set_visible_child(self->stack, GTK_WIDGET (self->split_view));
  gtk_revealer_set_reveal_child(self->edit_button_revealer, true);
  gtk_toggle_button_set_active(self->edit_button, true);
  adw_toolbar_view_set_top_bar_style(self->toolbar_view, ADW_TOOLBAR_RAISED_BORDER);
  self->image_opened = true;
}

static void
on_original_histogram (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
	PixelshopHistogramWindow *histogram_window = g_object_new (PIXELSHOP_TYPE_HISTOGRAM_WINDOW, NULL);

  int histogram[COLORS_PER_COMPONENT];

  pixelshop_image_get_original_histogram (self->image, histogram);
  pixelshop_histogram_window_set_data (histogram_window, histogram);

	gtk_window_present ((GtkWindow*) histogram_window);
}

static void
on_edited_histogram (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
	PixelshopHistogramWindow *histogram_window = g_object_new (PIXELSHOP_TYPE_HISTOGRAM_WINDOW, NULL);

  int histogram[COLORS_PER_COMPONENT];

  pixelshop_image_get_edited_histogram (self->image, histogram);
  pixelshop_histogram_window_set_data (histogram_window, histogram);

	gtk_window_present ((GtkWindow*) histogram_window);
}

static void
open_file_complete (GObject          *source_object,
                    GAsyncResult     *result,
                    PixelshopWindow  *self)
{
  GFile *file = G_FILE (source_object);

  g_autofree char *contents = NULL;
  gsize length = 0;

  g_autoptr (GError) error = NULL;

  // Complete the asynchronous operation; this function will either
  // give you the contents of the file as a byte array, or will
  // set the error argument
  g_file_load_contents_finish (file,
                               result,
                               &contents,
                               &length,
                               NULL,
                               &error);

  // In case of error, print a warning to the standard error output
  if (error != NULL)
    {
      g_printerr ("Unable to open “%s”: %s\n",
                  g_file_peek_path (file),
                  error->message);
      return;
    }

  if (!self->image_opened)
    stack_transition(self);

  reset_buttons(self);

  pixelshop_image_load_image ((guint8*) contents, length, self->image);

  draw_original_picture(self);
  draw_edited_picture(self);
 }

static void
open_file (PixelshopWindow *self,
           GFile *file)
{
  g_file_load_contents_async (file,
                              NULL,
                              (GAsyncReadyCallback) open_file_complete,
                              self);
}

static void
on_open_response (GObject *source,
                  GAsyncResult *result,
                  gpointer user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  PixelshopWindow *self = user_data;

  g_autoptr (GFile) file =
      gtk_file_dialog_open_finish (dialog, result, NULL);

  // If the user selected a file, open it
  if (file != NULL)
    open_file (self, file);
}

static void
pixelshop_window_open_file_dialog (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
  g_autoptr (GtkFileDialog) dialog = gtk_file_dialog_new ();
  g_autoptr (GtkFileFilter) filter = gtk_file_filter_new ();
  gtk_file_filter_add_mime_type (filter, "image/jpeg");
  gtk_file_filter_add_mime_type (filter, "image/png");
  gtk_file_dialog_set_default_filter(dialog, filter);

  gtk_file_dialog_open (dialog,
                        GTK_WINDOW (self),
                        NULL,
                        on_open_response,
                        self);
}

static gboolean
on_drop (GtkDropTarget *target,
         const GValue *value,
         double x,
         double y,
         gpointer data)
{
  PixelshopWindow *self = data;

  if (G_VALUE_HOLDS (value, G_TYPE_FILE))
    open_file (self, g_value_get_object (value));
  else
    return FALSE;

  return TRUE;
}

static void
save_file_complete (GObject      *source_object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GFile *file = G_FILE (source_object);

  g_autoptr (GError) error =  NULL;
  g_file_replace_contents_finish (file, result, NULL, &error);

  // Query the display name for the file
  g_autofree char *display_name = NULL;
  g_autoptr (GFileInfo) info =
  g_file_query_info (file,
                     "standard::display-name",
                     G_FILE_QUERY_INFO_NONE,
                     NULL,
                     NULL);
  if (info != NULL)
    {
      display_name =
        g_strdup (g_file_info_get_attribute_string (info, "standard::display-name"));
    }
  else
    {
      display_name = g_file_get_basename (file);
    }

  if (error != NULL)
    {
      g_printerr ("Unable to save “%s”: %s\n",
                  display_name,
                  error->message);
    }
}

static void
save_file (PixelshopWindow *self,
            GFile          *file)
{
  pixelshop_image_file_buffer context = pixelshop_image_export_as_jpg(self->image);

  g_file_replace_contents_async (
    file,
    context.context,
    context.last_pos,
    NULL,
    FALSE,
    G_FILE_CREATE_NONE,
    NULL,
    save_file_complete,
    self);
}

static void
on_save_response (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  PixelshopWindow *self = user_data;

  g_autoptr (GFile) file =
    gtk_file_dialog_save_finish (dialog, result, NULL);

  if (file != NULL)
    save_file (self, file);
}

static void
pixelshop_window_save_file_dialog (GAction          *action G_GNUC_UNUSED,
                                      GVariant         *param G_GNUC_UNUSED,
                                      PixelshopWindow *self)
{
  g_autoptr (GtkFileDialog) dialog =
    gtk_file_dialog_new ();

  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (self),
                        NULL,
                        on_save_response,
                        self);
}

static void
pixelshop_window_init (PixelshopWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->image_opened = false;

  self->image = g_object_new(PIXELSHOP_TYPE_IMAGE, NULL);

  // CSS:
  GtkCssProvider *css_provider = gtk_css_provider_new();
  gtk_css_provider_load_from_resource(css_provider, "/io/github/jspast/Pixelshop/main.css");
  GdkDisplay *display = gdk_display_get_default();
  gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

  // Open Button:
  g_autoptr (GSimpleAction) open_action = g_simple_action_new ("open", NULL);
  g_signal_connect (open_action, "activate", G_CALLBACK (pixelshop_window_open_file_dialog), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (open_action));

  // Save Button:
  g_autoptr (GSimpleAction) save_action = g_simple_action_new ("save-as", NULL);
  g_signal_connect (save_action, "activate", G_CALLBACK (pixelshop_window_save_file_dialog), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (save_action));

  // Drag and Drop:
  GtkDropTarget *target = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_COPY);
  gtk_drop_target_set_gtypes (target, (GType[1]) { G_TYPE_FILE, }, 1);
  g_signal_connect (target, "drop", G_CALLBACK (on_drop), self);
  gtk_widget_add_controller (GTK_WIDGET (self->stack), GTK_EVENT_CONTROLLER (target));

  // mirror_horizontally Button:
  g_signal_connect (self->hmirror_button, "clicked", G_CALLBACK (on_mirror_horizontally), self);

  // mirror_vertically Button:
  g_signal_connect (self->vmirror_button, "clicked", G_CALLBACK (on_mirror_vertically), self);

  // Grayscale Button:
  g_signal_connect (self->grayscale_button, "notify::active", G_CALLBACK (toggle_grayscale), self);

  // Colors Button:
  g_signal_connect (self->quantization_button, "output", G_CALLBACK (on_colors_changed), self);

  // Original Histogram Button:
  g_autoptr (GSimpleAction) original_histogram_action = g_simple_action_new ("original-histogram", NULL);
  g_signal_connect (original_histogram_action, "activate", G_CALLBACK (on_original_histogram), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (original_histogram_action));

  // Edited Histogram Button:
  g_autoptr (GSimpleAction) edited_histogram_action = g_simple_action_new ("edited-histogram", NULL);
  g_signal_connect (edited_histogram_action, "activate", G_CALLBACK (on_edited_histogram), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (edited_histogram_action));
}

