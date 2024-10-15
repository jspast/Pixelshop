#include "pixelshop-image.h"

#define GRAYSCALE_R 0.299
#define GRAYSCALE_G 0.587
#define GRAYSCALE_B 0.144

struct _PixelshopImage
{
  GObject parent_instance;

  gboolean mirrored_vertically;
  gboolean mirrored_horizontally;
  gboolean grayscale;
  guint16 quantization_colors;
  guint8 min_color;
  guint8 max_color;

  GdkTexture *original_texture;

  GByteArray *edited_buffer;
  gsize height;
  gsize width;
  gsize stride;
};

G_DEFINE_FINAL_TYPE (PixelshopImage, pixelshop_image, G_TYPE_OBJECT)

static void
mirror_horizontally (PixelshopImage *self)
{
  guint8 pixel_buffer;
  gsize line_offset;
  gsize line_end_offset;
  gsize pixel_offset;

  for (gsize i = 0; i < self->height; i++) {
    line_offset = i * self->stride;
    line_end_offset = line_offset + self->stride - 3;

    for (gsize j = 0; j < self->stride / 6; j++) {
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
mirror_vertically (PixelshopImage *self)
{
  guint8 line_buffer[self->stride];
  gsize line_offset;
  gsize last_line_offset = self->edited_buffer->len - self->stride;

  for (gsize i = 0; i < self->height / 2; i++) {
    line_offset = i * self->stride;
    memcpy(line_buffer, &self->edited_buffer->data[line_offset], self->stride);
    memcpy(&self->edited_buffer->data[line_offset], &self->edited_buffer->data[last_line_offset - line_offset], self->stride);
    memcpy(&self->edited_buffer->data[last_line_offset - line_offset], line_buffer, self->stride);
  }
}

static void
apply_mirror (PixelshopImage *self)
{
  if (self->mirrored_vertically)
    mirror_vertically(self);

  if (self->mirrored_horizontally)
    mirror_horizontally(self);
}

static void
check_colors (PixelshopImage *self)
{
  gsize line_offset;
  gboolean colors[256] = {false};

  for (gsize i = 0; i < self->height; i++) {
    line_offset = i * self->stride;

    for (gsize j = 0; j < self->stride; j += 3) {

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
apply_quantization (PixelshopImage *self)
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

    for (gsize m = 0; m < self->height; m++) {
      line_offset = m * self->stride;

      for (gsize j = 0; j < self->stride; j += 3) {

        self->edited_buffer->data[line_offset + j] = map[self->edited_buffer->data[line_offset + j]];
        self->edited_buffer->data[line_offset + j + 1] = map[self->edited_buffer->data[line_offset + j + 1]];
        self->edited_buffer->data[line_offset + j + 2] = map[self->edited_buffer->data[line_offset + j + 2]];
      }
    }
  }
}

static void
update_edited_picture_from_original (PixelshopImage *self)
{
  GdkTextureDownloader *texture_downloader = gdk_texture_downloader_new (self->original_texture);
  gdk_texture_downloader_set_format(texture_downloader, GDK_MEMORY_R8G8B8);
  GBytes *tmp_buffer = gdk_texture_downloader_download_bytes(texture_downloader, &self->stride);
  self->edited_buffer = g_bytes_unref_to_array(tmp_buffer);
}

void
pixelshop_image_load_image (GFile *file, PixelshopImage *self)
{
  self->original_texture = gdk_texture_new_from_file (file, NULL);
  self->height = gdk_texture_get_height(self->original_texture);
  self->width = gdk_texture_get_width(self->original_texture);

  update_edited_picture_from_original (self);

  check_colors (self);
}

GdkTexture*
pixelshop_image_get_original_texture (PixelshopImage *self)
{
  return self->original_texture;
}

GdkTexture*
pixelshop_image_get_edited_texture (PixelshopImage *self)
{
  GBytes *tmp_bytes = g_bytes_new_take(self->edited_buffer->data, self->edited_buffer->len);

  GdkTexture *edited_texture = gdk_memory_texture_new(self->width, self->height, GDK_MEMORY_R8G8B8, tmp_bytes, self->stride);

  return edited_texture;
}

void
pixelshop_image_reset_edits (PixelshopImage *self)
{
  self->grayscale = false;

  self->quantization_colors = 256;

  self->mirrored_horizontally = false;
  self->mirrored_vertically = false;
}

static void
apply_grayscale (PixelshopImage *self)
{
  gsize line_offset;
  guint8 red, green, blue, lum;

  for (gsize i = 0; i < self->height; i++) {
    line_offset = i * self->stride;

    for (gsize j = 0; j < self->stride; j += 3) {

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
revert_grayscale (PixelshopImage *self)
{
  update_edited_picture_from_original (self);
  apply_mirror(self);
}

void
pixelshop_image_toggle_mirrored_horizontally (PixelshopImage *self)
{
  self->mirrored_horizontally = !self->mirrored_horizontally;

  mirror_horizontally (self);
}

void
pixelshop_image_toggle_mirrored_vertically (PixelshopImage *self)
{
  self->mirrored_vertically = !self->mirrored_vertically;

  mirror_vertically (self);
}

void
pixelshop_image_toggle_grayscale (PixelshopImage *self)
{
  self->grayscale = !self->grayscale;

  if (self->grayscale)
    apply_grayscale(self);
  else
    revert_grayscale(self);
}

void
pixelshop_image_set_quantization_colors (int colors, PixelshopImage *self)
{
  self->quantization_colors = colors;

  revert_grayscale(self);
  apply_grayscale (self);
}

gsize
pixelshop_image_get_width (PixelshopImage *self)
{
  return self->width;
}

gsize
pixelshop_image_get_height (PixelshopImage *self)
{
  return self->height;
}

GByteArray*
pixelshop_image_get_edited_buffer (PixelshopImage *self)
{
  return self->edited_buffer;
}

static void
pixelshop_image_class_init (PixelshopImageClass *klass)
{

}

static void
pixelshop_image_init (PixelshopImage *self)
{

}

