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
  GtkToggleButton *sidebar_button;
  AdwOverlaySplitView *split_view;

  GtkToggleButton *hmirror_button;
  GtkToggleButton *vmirror_button;
  GtkSwitch *grayscale_button;
  AdwSpinRow *quantization_button;
  AdwSpinRow *brightness_button;
  AdwSpinRow *contrast_button;
  AdwSwitchRow *negative_button;
  AdwSwitchRow *equalize_button;
  GtkSwitch *match_row;
  GtkToggleButton *zoom_out_button;
  AdwSpinRow *zoom_out_x_factor;
  AdwSpinRow *zoom_out_y_factor;
  GtkToggleButton *zoom_in_button;
  GtkToggleButton *rotate_left_button;
  GtkToggleButton *rotate_right_button;

  GtkGrid *filter_grid;
  AdwExpanderRow *filter_row;
  GtkDropDown *filter_drop_down;
  GtkSwitch *filter_switch;
  GtkEntryBuffer *filter_buffer0;
  GtkEntryBuffer *filter_buffer1;
  GtkEntryBuffer *filter_buffer2;
  GtkEntryBuffer *filter_buffer3;
  GtkEntryBuffer *filter_buffer4;
  GtkEntryBuffer *filter_buffer5;
  GtkEntryBuffer *filter_buffer6;
  GtkEntryBuffer *filter_buffer7;
  GtkEntryBuffer *filter_buffer8;
  AdwSpinRow *filter_add_button;
  AdwSwitchRow *filter_colored_button;

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

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/io/github/jspast/Pixelshop/pixelshop-window.ui");

  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, toolbar_view);

  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, stack);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, edit_button_revealer);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, sidebar_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, split_view);

  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, hmirror_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, vmirror_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, grayscale_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, quantization_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, brightness_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, contrast_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, negative_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, equalize_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, match_row);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, zoom_out_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, zoom_out_x_factor);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, zoom_out_y_factor);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, zoom_in_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, rotate_left_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, rotate_right_button);

  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, filter_grid);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, filter_row);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, filter_drop_down);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, filter_switch);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, filter_buffer0);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, filter_buffer1);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, filter_buffer2);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, filter_buffer3);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, filter_buffer4);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, filter_buffer5);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, filter_buffer6);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, filter_buffer7);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, filter_buffer8);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, filter_add_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, filter_colored_button);

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
reset_buttons (PixelshopWindow *self)
{
  gtk_toggle_button_set_active (self->hmirror_button, false);
  gtk_toggle_button_set_active (self->vmirror_button, false);
  gtk_switch_set_active (self->grayscale_button, false);
  adw_spin_row_set_value(self->quantization_button, COLORS_PER_COMPONENT);
  adw_spin_row_set_value (self->brightness_button, 0);
  adw_spin_row_set_value (self->contrast_button, 1);
  adw_switch_row_set_active (self->negative_button, false);
  adw_switch_row_set_active (self->equalize_button, false);
  gtk_switch_set_active (self->match_row, false);
  gtk_toggle_button_set_active (self->zoom_out_button, false);
  adw_spin_row_set_value (self->zoom_out_x_factor, 2);
  adw_spin_row_set_value (self->zoom_out_y_factor, 2);
  gtk_toggle_button_set_active (self->zoom_in_button, false);
  gtk_toggle_button_set_active (self->rotate_left_button, false);
  gtk_toggle_button_set_active (self->rotate_right_button, false);
  gtk_switch_set_active (self->filter_switch, false);
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
stack_transition (PixelshopWindow *self)
{
  gtk_stack_set_visible_child(self->stack, GTK_WIDGET (self->split_view));
  gtk_revealer_set_reveal_child(self->edit_button_revealer, true);
  gtk_toggle_button_set_active(self->sidebar_button, true);
  adw_toolbar_view_set_top_bar_style(self->toolbar_view, ADW_TOOLBAR_RAISED_BORDER);
  self->image_opened = true;

  // Save Button:
  g_autoptr (GSimpleAction) save_action = g_simple_action_new ("save-as", NULL);
  g_signal_connect (save_action, "activate",
                    G_CALLBACK (pixelshop_window_save_file_dialog), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (save_action));
}

static void
on_original_histogram (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
	PixelshopHistogramWindow *hist_window = g_object_new (PIXELSHOP_TYPE_HISTOGRAM_WINDOW, NULL);

  int histogram[COLORS_PER_COMPONENT];

  pixelshop_image_get_original_histogram (self->image, histogram);
  pixelshop_histogram_window_set_data (hist_window, histogram);

	gtk_window_present ((GtkWindow*) hist_window);
}

static void
on_edited_histogram (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
	PixelshopHistogramWindow *hist_window = g_object_new (PIXELSHOP_TYPE_HISTOGRAM_WINDOW, NULL);

  int histogram[COLORS_PER_COMPONENT];

  pixelshop_image_get_edited_histogram (self->image, histogram);
  pixelshop_histogram_window_set_data (hist_window, histogram);

	gtk_window_present ((GtkWindow*) hist_window);
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
pixelshop_window_open_file_dialog (GAction *action, GVariant *parameter,
                                   PixelshopWindow *self)
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
open_target_file_complete (GObject          *source_object,
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

  pixelshop_image_load_target_image (self->image, (guint8*)contents, length);

  gtk_widget_set_sensitive (GTK_WIDGET (self->match_row), true);
 }

static void
open_target_file (PixelshopWindow *self,
           GFile *file)
{
  g_file_load_contents_async (file,
                              NULL,
                              (GAsyncReadyCallback) open_target_file_complete,
                              self);
}

static void
on_open_target_response (GObject *source,
                  GAsyncResult *result,
                  gpointer user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  PixelshopWindow *self = user_data;

  g_autoptr (GFile) file =
      gtk_file_dialog_open_finish (dialog, result, NULL);

  // If the user selected a file, open it
  if (file != NULL)
    open_target_file (self, file);
}

static void
pixelshop_window_open_target_file_dialog (GAction *action, GVariant *parameter,
                                          PixelshopWindow *self)
{
  g_autoptr (GtkFileDialog) dialog = gtk_file_dialog_new ();
  g_autoptr (GtkFileFilter) filter = gtk_file_filter_new ();
  gtk_file_filter_add_mime_type (filter, "image/jpeg");
  gtk_file_filter_add_mime_type (filter, "image/png");
  gtk_file_dialog_set_default_filter(dialog, filter);

  gtk_file_dialog_open (dialog,
                        GTK_WINDOW (self),
                        NULL,
                        on_open_target_response,
                        self);
}

static void
get_filter (PixelshopWindow *self, float *filter)
{
  filter[0] = atof (gtk_entry_buffer_get_text (self->filter_buffer0));
  filter[1] = atof (gtk_entry_buffer_get_text (self->filter_buffer1));
  filter[2] = atof (gtk_entry_buffer_get_text (self->filter_buffer2));
  filter[3] = atof (gtk_entry_buffer_get_text (self->filter_buffer3));
  filter[4] = atof (gtk_entry_buffer_get_text (self->filter_buffer4));
  filter[5] = atof (gtk_entry_buffer_get_text (self->filter_buffer5));
  filter[6] = atof (gtk_entry_buffer_get_text (self->filter_buffer6));
  filter[7] = atof (gtk_entry_buffer_get_text (self->filter_buffer7));
  filter[8] = atof (gtk_entry_buffer_get_text (self->filter_buffer8));
}

static void
on_apply (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
  if (gtk_toggle_button_get_active (self->hmirror_button))
    pixelshop_image_mirror_horizontally (self->image);

  if (gtk_toggle_button_get_active (self->vmirror_button))
    pixelshop_image_mirror_vertically (self->image);

  if (gtk_switch_get_active (self->grayscale_button))
    pixelshop_image_apply_quantized_grayscale (self->image,
                                               adw_spin_row_get_value (self->quantization_button));

  pixelshop_image_apply_brightness (self->image, adw_spin_row_get_value (self->brightness_button));

  pixelshop_image_apply_contrast (self->image, adw_spin_row_get_value (self->contrast_button));

  if (adw_switch_row_get_active (self->negative_button))
    pixelshop_image_apply_negative (self->image);

  if (adw_switch_row_get_active (self->equalize_button))
    pixelshop_image_equalize (self->image);

  if (gtk_switch_get_active (self->match_row))
    pixelshop_image_match (self->image);

  if (gtk_toggle_button_get_active (self->zoom_out_button))
    pixelshop_image_zoom_out (self->image,
                              adw_spin_row_get_value (self->zoom_out_x_factor),
                              adw_spin_row_get_value (self->zoom_out_y_factor));

  if (gtk_toggle_button_get_active (self->zoom_in_button))
    pixelshop_image_zoom_in (self->image);

  if (gtk_toggle_button_get_active (self->rotate_left_button))
    pixelshop_image_rotate_left (self->image);

  if (gtk_toggle_button_get_active (self->rotate_right_button))
    pixelshop_image_rotate_right (self->image);

  if (gtk_switch_get_active (self->filter_switch)) {
    float filter[9];
    get_filter (self, filter);
    pixelshop_image_apply_filter (self->image, filter,
                                  adw_switch_row_get_active (self->filter_colored_button),
                                  adw_spin_row_get_value (self->filter_add_button));
  }

  draw_edited_picture (self);
  reset_buttons (self);
}

enum filters {
  GAUSSIAN,
  LAPLACIAN,
  HIGH_PASS,
  PREWITT_HX,
  PREWITT_HY,
  SOBEL_HX,
  SOBEL_HY,
  CUSTOM,
};

static void
on_filter_selected (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
  gtk_switch_set_active (self->filter_switch, true);

  gtk_widget_set_sensitive (GTK_WIDGET (self->filter_grid), false);

  adw_spin_row_set_value (self->filter_add_button, 0);
  adw_switch_row_set_active (self->filter_colored_button, false);

  switch (gtk_drop_down_get_selected (self->filter_drop_down)) {
  case GAUSSIAN:
    gtk_entry_buffer_set_text (self->filter_buffer0, "0.0625", 6);
    gtk_entry_buffer_set_text (self->filter_buffer1, "0.125", 5);
    gtk_entry_buffer_set_text (self->filter_buffer2, "0.0625", 6);
    gtk_entry_buffer_set_text (self->filter_buffer3, "0.125", 5);
    gtk_entry_buffer_set_text (self->filter_buffer4, "0.25", 4);
    gtk_entry_buffer_set_text (self->filter_buffer5, "0.125", 5);
    gtk_entry_buffer_set_text (self->filter_buffer6, "0.0625", 6);
    gtk_entry_buffer_set_text (self->filter_buffer7, "0.125", 5);
    gtk_entry_buffer_set_text (self->filter_buffer8, "0.0625", 6);
    adw_switch_row_set_active (self->filter_colored_button, true);
    break;
  case LAPLACIAN:
    gtk_entry_buffer_set_text (self->filter_buffer0, "0", 1);
    gtk_entry_buffer_set_text (self->filter_buffer1, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer2, "0", 1);
    gtk_entry_buffer_set_text (self->filter_buffer3, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer4, "4", 1);
    gtk_entry_buffer_set_text (self->filter_buffer5, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer6, "0", 1);
    gtk_entry_buffer_set_text (self->filter_buffer7, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer8, "0", 1);
    break;
  case HIGH_PASS:
    gtk_entry_buffer_set_text (self->filter_buffer0, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer1, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer2, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer3, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer4, "8", 1);
    gtk_entry_buffer_set_text (self->filter_buffer5, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer6, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer7, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer8, "-1", 2);
    break;
  case PREWITT_HX:
    gtk_entry_buffer_set_text (self->filter_buffer0, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer1, "0", 1);
    gtk_entry_buffer_set_text (self->filter_buffer2, "1", 1);
    gtk_entry_buffer_set_text (self->filter_buffer3, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer4, "0", 1);
    gtk_entry_buffer_set_text (self->filter_buffer5, "1", 1);
    gtk_entry_buffer_set_text (self->filter_buffer6, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer7, "0", 1);
    gtk_entry_buffer_set_text (self->filter_buffer8, "1", 1);
    adw_spin_row_set_value (self->filter_add_button, 127);
    break;
  case PREWITT_HY:
    gtk_entry_buffer_set_text (self->filter_buffer0, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer1, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer2, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer3, "0", 1);
    gtk_entry_buffer_set_text (self->filter_buffer4, "0", 1);
    gtk_entry_buffer_set_text (self->filter_buffer5, "0", 1);
    gtk_entry_buffer_set_text (self->filter_buffer6, "1", 1);
    gtk_entry_buffer_set_text (self->filter_buffer7, "1", 1);
    gtk_entry_buffer_set_text (self->filter_buffer8, "1", 1);
    adw_spin_row_set_value (self->filter_add_button, 127);
    break;
  case SOBEL_HX:
    gtk_entry_buffer_set_text (self->filter_buffer0, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer1, "0", 1);
    gtk_entry_buffer_set_text (self->filter_buffer2, "1", 1);
    gtk_entry_buffer_set_text (self->filter_buffer3, "-2", 2);
    gtk_entry_buffer_set_text (self->filter_buffer4, "0", 1);
    gtk_entry_buffer_set_text (self->filter_buffer5, "2", 1);
    gtk_entry_buffer_set_text (self->filter_buffer6, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer7, "0", 1);
    gtk_entry_buffer_set_text (self->filter_buffer8, "1", 1);
    adw_spin_row_set_value (self->filter_add_button, 127);
    break;
  case SOBEL_HY:
    gtk_entry_buffer_set_text (self->filter_buffer0, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer1, "-2", 2);
    gtk_entry_buffer_set_text (self->filter_buffer2, "-1", 2);
    gtk_entry_buffer_set_text (self->filter_buffer3, "0", 1);
    gtk_entry_buffer_set_text (self->filter_buffer4, "0", 1);
    gtk_entry_buffer_set_text (self->filter_buffer5, "0", 1);
    gtk_entry_buffer_set_text (self->filter_buffer6, "1", 1);
    gtk_entry_buffer_set_text (self->filter_buffer7, "2", 1);
    gtk_entry_buffer_set_text (self->filter_buffer8, "1", 1);
    adw_spin_row_set_value (self->filter_add_button, 127);
    break;
  default:
    adw_expander_row_set_expanded (self->filter_row, true);
    gtk_widget_set_sensitive (GTK_WIDGET (self->filter_grid), true);
    break;
  }
}

static void
on_reset (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
  pixelshop_image_reset_edited_image (self->image);

  draw_edited_picture (self);
  reset_buttons (self);
}

static void
on_zoom_in_selected (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
  if (gtk_toggle_button_get_active (self->zoom_in_button))
    gtk_toggle_button_set_active (self->zoom_out_button, false);
}

static void
on_zoom_out_selected (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
  if (gtk_toggle_button_get_active (self->zoom_out_button))
    gtk_toggle_button_set_active (self->zoom_in_button, false);
}

static void
on_rotate_left_selected (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
  if (gtk_toggle_button_get_active (self->rotate_left_button))
    gtk_toggle_button_set_active (self->rotate_right_button, false);
}

static void
on_rotate_right_selected (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
  if (gtk_toggle_button_get_active (self->rotate_right_button))
    gtk_toggle_button_set_active (self->rotate_left_button, false);
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
  gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(css_provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);

  // Open Button:
  g_autoptr (GSimpleAction) open_action = g_simple_action_new ("open", NULL);
  g_signal_connect (open_action, "activate",
                    G_CALLBACK (pixelshop_window_open_file_dialog), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (open_action));

  // Drag and Drop:
  GtkDropTarget *target = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_COPY);
  gtk_drop_target_set_gtypes (target, (GType[1]) { G_TYPE_FILE, }, 1);
  g_signal_connect (target, "drop", G_CALLBACK (on_drop), self);
  gtk_widget_add_controller (GTK_WIDGET (self->stack), GTK_EVENT_CONTROLLER (target));

  // Apply Button:
  g_autoptr (GSimpleAction) apply_action = g_simple_action_new ("apply", NULL);
  g_signal_connect (apply_action, "activate", G_CALLBACK (on_apply), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (apply_action));

  // Reset Button:
  g_autoptr (GSimpleAction) reset_action = g_simple_action_new ("reset", NULL);
  g_signal_connect (reset_action, "activate", G_CALLBACK (on_reset), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (reset_action));

  // Open Target File Button:
  g_autoptr (GSimpleAction) open_target_action = g_simple_action_new ("open-target",
                                                                      NULL);
  g_signal_connect (open_target_action, "activate",
                    G_CALLBACK (pixelshop_window_open_target_file_dialog), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (open_target_action));

  // Zoom and Rotate Buttons behaviour
  g_signal_connect (self->zoom_in_button, "toggled",
                    G_CALLBACK (on_zoom_in_selected), self);
  g_signal_connect (self->zoom_out_button, "toggled",
                    G_CALLBACK (on_zoom_out_selected), self);
  g_signal_connect (self->rotate_left_button, "toggled",
                    G_CALLBACK (on_rotate_left_selected), self);
  g_signal_connect (self->rotate_right_button, "toggled",
                    G_CALLBACK (on_rotate_right_selected), self);

  // Filter Selection
  g_signal_connect (self->filter_drop_down, "notify::selected", G_CALLBACK (on_filter_selected), self);

  // Original Histogram Button:
  g_autoptr (GSimpleAction) original_histogram_action = g_simple_action_new ("original-histogram",
                                                                             NULL);
  g_signal_connect (original_histogram_action, "activate",
                    G_CALLBACK (on_original_histogram), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (original_histogram_action));

  // Edited Histogram Button:
  g_autoptr (GSimpleAction) edited_histogram_action = g_simple_action_new ("edited-histogram",
                                                                           NULL);
  g_signal_connect (edited_histogram_action, "activate",
                    G_CALLBACK (on_edited_histogram), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (edited_histogram_action));
}

