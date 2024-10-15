#include "config.h"

#include "pixelshop-window.h"
#include "pixelshop-image.h"

#define GRAYSCALE_R 0.299
#define GRAYSCALE_G 0.587
#define GRAYSCALE_B 0.144

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

  PixelshopImage *image;

  gboolean mirrored_vertically;
  gboolean mirrored_horizontally;
  gboolean grayscale;
  guint16 quantization_colors;
  guint8 min_color;
  guint8 max_color;

  GtkBox *pictures_box;
  GtkPicture *original_picture;
  GtkPicture *edited_picture;

  GdkTexture *original_texture;

  GByteArray *edited_buffer;
  gsize buffer_height;
  gsize buffer_width;
  gsize buffer_stride;
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
  gtk_picture_set_paintable(self->original_picture, GDK_PAINTABLE(self->original_texture));
}

static void
draw_edited_picture (PixelshopWindow *self)
{
  GBytes *tmp_bytes = g_bytes_new_take(self->edited_buffer->data, self->edited_buffer->len);

  GdkTexture *edited_texture = gdk_memory_texture_new(self->buffer_width, self->buffer_height, GDK_MEMORY_R8G8B8, tmp_bytes, self->buffer_stride);

  gtk_picture_set_paintable(self->edited_picture, GDK_PAINTABLE(edited_texture));
}

static void
update_edited_picture_from_original (PixelshopWindow *self)
{
  GdkTextureDownloader *texture_downloader = gdk_texture_downloader_new (self->original_texture);
  gdk_texture_downloader_set_format(texture_downloader, GDK_MEMORY_R8G8B8);
  GBytes *tmp_buffer = gdk_texture_downloader_download_bytes(texture_downloader, &self->buffer_stride);
  self->edited_buffer = g_bytes_unref_to_array(tmp_buffer);
}

static void
mirror_horizontally (PixelshopWindow *self)
{
  guint8 pixel_buffer;
  gsize line_offset;
  gsize line_end_offset;
  gsize pixel_offset;

  for (gsize i = 0; i < self->buffer_height; i++) {
    line_offset = i * self->buffer_stride;
    line_end_offset = line_offset + self->buffer_stride - 3;

    for (gsize j = 0; j < self->buffer_stride / 6; j++) {
      pixel_offset = j * 3;

      for (gsize k = 0; k < 3; k++) {
        pixel_buffer = self->edited_buffer->data[line_offset + pixel_offset + k];
        self->edited_buffer->data[line_offset + pixel_offset + k] = self->edited_buffer->data[line_end_offset - pixel_offset + k];
        self->edited_buffer->data[line_end_offset - pixel_offset + k] = pixel_buffer;
      }
    }
  }
}

static void
mirror_vertically (PixelshopWindow *self)
{
  guint8 line_buffer[self->buffer_stride];
  gsize line_offset;
  gsize last_line_offset = self->edited_buffer->len - self->buffer_stride;

  for (gsize i = 0; i < self->buffer_height / 2; i++) {
    line_offset = i * self->buffer_stride;
    memcpy(line_buffer, &self->edited_buffer->data[line_offset], self->buffer_stride);
    memcpy(&self->edited_buffer->data[line_offset], &self->edited_buffer->data[last_line_offset - line_offset], self->buffer_stride);
    memcpy(&self->edited_buffer->data[last_line_offset - line_offset], line_buffer, self->buffer_stride);
  }
}

static void
on_mirror_horizontally (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
  self->mirrored_horizontally = !self->mirrored_horizontally;
  mirror_horizontally (self);
  draw_edited_picture (self);
}

static void
on_mirror_vertically (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
  self->mirrored_vertically = !self->mirrored_vertically;
  mirror_vertically (self);
  draw_edited_picture (self);
}

static void
apply_mirror (PixelshopWindow *self)
{
  if (self->mirrored_vertically)
    mirror_vertically(self);

  if (self->mirrored_horizontally)
    mirror_horizontally(self);
}

static void
check_colors (PixelshopWindow *self)
{
  gsize line_offset;
  gboolean colors[256] = {false};

  for (gsize i = 0; i < self->buffer_height; i++) {
    line_offset = i * self->buffer_stride;

    for (gsize j = 0; j < self->buffer_stride; j += 3) {

      colors[self->edited_buffer->data[line_offset + j]] = true;
    }
  }

  gint16 i = 0;
  gboolean done = false;

  while (!done && i < 256) {
    if (colors[i])
      done = true;
    else
      i++;
  }

  self->min_color = MIN(i, 255);
  i = 255;

  while (!done && i > 0) {
    if (colors[i])
      done = true;
    else
      i--;
  }

  self->max_color = MAX(i, 0);
}

static void
apply_quantization (PixelshopWindow *self)
{
  guint16 range_size = self->max_color - self->min_color + 1;

  if (self->quantization_colors < range_size) {

    gdouble bin_size = (gdouble)range_size / (gdouble)self->quantization_colors;

    guint8 map[256] = {0};

    gdouble end = self->min_color - 0.5 + bin_size;
    gdouble mid = end - bin_size / 2;

    guint16 k = 0;
    guint16 i;

    for (i = 0; i < self->quantization_colors && k < 256; i++) {

      while (k < end) {
        map[k] = MIN(mid, 255);
        k++;
      }

      end += bin_size;
      mid += bin_size;
    }

    while (k < 256) {
      map[k] = MIN(mid, 255);
      k++;
    }

    gsize line_offset;

    for (gsize m = 0; m < self->buffer_height; m++) {
      line_offset = m * self->buffer_stride;

      for (gsize j = 0; j < self->buffer_stride; j += 3) {

        self->edited_buffer->data[line_offset + j] = map[self->edited_buffer->data[line_offset + j]];
        self->edited_buffer->data[line_offset + j + 1] = map[self->edited_buffer->data[line_offset + j + 1]];
        self->edited_buffer->data[line_offset + j + 2] = map[self->edited_buffer->data[line_offset + j + 2]];
      }
    }
  }
}

static void
apply_grayscale (PixelshopWindow *self)
{
  gsize line_offset;
  guint8 red, green, blue, lum;

  for (gsize i = 0; i < self->buffer_height; i++) {
    line_offset = i * self->buffer_stride;

    for (gsize j = 0; j < self->buffer_stride; j += 3) {

      red = self->edited_buffer->data[line_offset + j] * GRAYSCALE_R;
      green = self->edited_buffer->data[line_offset + j + 1] * GRAYSCALE_G;
      blue = self->edited_buffer->data[line_offset + j + 2] * GRAYSCALE_B;

      lum = MIN(red + green + blue, 255);

      self->edited_buffer->data[line_offset + j] = lum;
      self->edited_buffer->data[line_offset + j + 1] = lum;
      self->edited_buffer->data[line_offset + j + 2] = lum;
    }
  }

  apply_quantization (self);
}


static void
revert_grayscale (PixelshopWindow *self)
{
  update_edited_picture_from_original (self);
  apply_mirror(self);
}

static void
toggle_grayscale (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
  self->grayscale = !self->grayscale;

  if (self->grayscale)
    apply_grayscale(self);
  else
    revert_grayscale(self);

  draw_edited_picture (self);
}

static void
on_colors_changed (GAction *action, GVariant *parameter, PixelshopWindow *self)
{
  self->quantization_colors = gtk_spin_button_get_value_as_int(self->quantization_button);

  revert_grayscale(self);
  apply_grayscale (self);
  draw_edited_picture (self);
}

static void
reset_buttons (PixelshopWindow *self)
{
  gtk_toggle_button_set_active(self->grayscale_button, false);
  self->grayscale = false;

  gtk_spin_button_set_value(self->quantization_button, 256);
  self->quantization_colors = 256;

  self->mirrored_horizontally = false;
  self->mirrored_vertically = false;
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

  self->original_texture = gdk_texture_new_from_file (file, NULL);
  self->buffer_height = gdk_texture_get_height(self->original_texture);
  self->buffer_width = gdk_texture_get_width(self->original_texture);

  update_edited_picture_from_original(self);

  check_colors(self);

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

