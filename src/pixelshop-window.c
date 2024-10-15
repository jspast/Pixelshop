#include "config.h"

#include "pixelshop-window.h"
#include "pixelshop-image.h"

struct _PixelshopWindow
{
  AdwApplicationWindow parent_instance;

  gboolean image_opened;

  AdwToolbarView *toolbar_view;

  GtkStack *stack;
  GtkRevealer *edit_button_revealer;
  GtkToggleButton *edit_button;

  GtkToggleButton *grayscale_button;
  GtkSpinButton *quantization_button;

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

  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, grayscale_button);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, quantization_button);

  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, pictures_box);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, original_picture);
  gtk_widget_class_bind_template_child (widget_class, PixelshopWindow, edited_picture);
}

static void
draw_original_picture (PixelshopWindow *self)
{
  gtk_picture_set_paintable(self->original_picture, GDK_PAINTABLE(pixelshop_image_get_original_texture(self->image)));
}

static void
draw_edited_picture (PixelshopWindow *self)
{
  gtk_picture_set_paintable(self->edited_picture, GDK_PAINTABLE(pixelshop_image_get_edited_texture(self->image)));
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
  pixelshop_image_set_quantization_colors (gtk_spin_button_get_value_as_int(self->quantization_button), self->image);

  draw_edited_picture (self);
}

static void
reset_buttons (PixelshopWindow *self)
{
  gtk_toggle_button_set_active(self->grayscale_button, false);
  gtk_spin_button_set_value(self->quantization_button, 256);
}

static void
stack_transition (PixelshopWindow *self)
{
  gtk_stack_set_visible_child(self->stack, GTK_WIDGET (self->pictures_box));
  gtk_revealer_set_reveal_child(self->edit_button_revealer, true);
  gtk_toggle_button_set_active(self->edit_button, true);
  adw_toolbar_view_set_top_bar_style(self->toolbar_view, ADW_TOOLBAR_RAISED_BORDER);
  self->image_opened = true;
}

static void
open_file (PixelshopWindow *self,
           GFile *file)
{
  if (!self->image_opened)
    stack_transition(self);

  reset_buttons(self);
  pixelshop_image_reset_edits(self->image);

  pixelshop_image_load_image (file, self->image);

  draw_original_picture(self);
  draw_edited_picture(self);
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
  /* GdkFileList is a boxed value so we use the boxed API. */
  GdkFileList *file_list = g_value_get_boxed (value);

  GSList *list = gdk_file_list_get_files (file_list);

  GFile* file = list->data;

  PixelshopWindow *self = data;

  if (file != NULL)
    open_file (self, file);

  return TRUE;
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

  // Drag and Drop:
  GtkDropTarget *target = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_COPY);
  gtk_drop_target_set_gtypes (target, (GType[1]) { GDK_TYPE_FILE_LIST, }, 1);
  g_signal_connect (target, "drop", G_CALLBACK (on_drop), self);
  gtk_widget_add_controller (GTK_WIDGET (self->stack), GTK_EVENT_CONTROLLER (target));

  // mirror_horizontally Button:
  g_autoptr (GSimpleAction) mirror_horizontally_action = g_simple_action_new ("mirror-horizontally", NULL);
  g_signal_connect (mirror_horizontally_action, "activate", G_CALLBACK (on_mirror_horizontally), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (mirror_horizontally_action));

  // mirror_vertically Button:
  g_autoptr (GSimpleAction) mirror_vertically_action = g_simple_action_new ("mirror-vertically", NULL);
  g_signal_connect (mirror_vertically_action, "activate", G_CALLBACK (on_mirror_vertically), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (mirror_vertically_action));

  // Grayscale Button:
  g_signal_connect (self->grayscale_button, "toggled", G_CALLBACK (toggle_grayscale), self);

  // Colors Button:
  g_signal_connect (self->quantization_button, "value-changed", G_CALLBACK (on_colors_changed), self);
}

