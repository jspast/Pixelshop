#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "include/stb_image_write.h"

#include "pixelshop-image.h"

struct _PixelshopImage
{
  GObject parent_instance;

  // Stores the image the way it was originally loaded from a file
  guint8 *original_image;

  // Stores an scaled version of the original image as an array of bytes
  // Is used as a reference to apply filters on a potentially zoomed image
  guint8 *scaled_image;

  // Stores the edited image as an array of bytes, with scale and filters applied
  guint8 *edited_image;

  // Original image properties
  int original_height;
  int original_width;
  gsize original_stride;
  gsize original_length;
  guint8 min_luminance;
  guint8 max_luminance;
  int original_histogram[COLORS_PER_COMPONENT];

  // Edited image properties
  int scaled_height;
  int scaled_width;
  gsize scaled_stride;
  gsize scaled_length;
  gboolean mirrored_vertically;
  gboolean mirrored_horizontally;
  gboolean grayscale;
  int quantization_colors;
};

G_DEFINE_FINAL_TYPE (PixelshopImage, pixelshop_image, G_TYPE_OBJECT)

static void
pixelshop_image_reset_scale_properties (PixelshopImage *self)
{
  self->scaled_height = self->original_height;
  self->scaled_width = self->original_width;
  self->scaled_stride = self->original_stride;
  self->scaled_length = self->original_length;
}

static void
pixelshop_image_reset_scaled_image_from_original (PixelshopImage *self)
{
  pixelshop_image_reset_scale_properties (self);

  self->scaled_image = realloc (self->scaled_image, self->scaled_length);

  memcpy (self->scaled_image, self->original_image, self->original_length);
}

static void
pixelshop_image_reset_edited_image_properties (PixelshopImage *self)
{
  self->grayscale = false;

  self->mirrored_horizontally = false;
  self->mirrored_vertically = false;
}

static void
pixelshop_image_reset_edited_image_from_scaled (PixelshopImage *self)
{
  self->edited_image = realloc (self->edited_image, self->scaled_length);

  memcpy (self->edited_image, self->scaled_image, self->scaled_length);
}

static void
pixelshop_image_reset_edited_image (PixelshopImage *self)
{
  pixelshop_image_reset_scaled_image_from_original (self);
  pixelshop_image_reset_edited_image_from_scaled (self);
}

static void
pixelshop_image_calculate_histogram_from_grayscale_image (guint8 *image,
                                                          gsize image_length,
                                                          int *histogram)
{
  memset(histogram, 0, COLORS_PER_COMPONENT * sizeof(int));

  for (gsize i = 0; i < image_length; i += COMPONENTS) {
    histogram[image[i]]++;
  }
}

static guint8
pixelshop_image_calculate_luminance_from_pixel (guint8 *pixel)
{
  float lum = pixel[0] * GRAYSCALE_R +
              pixel[1] * GRAYSCALE_G +
              pixel[2] * GRAYSCALE_B;

  return (guint8) MIN(lum, 255);
}

static void
pixelshop_image_calculate_histogram_from_colored_image (guint8 *image,
                                                        gsize image_length,
                                                        int *histogram)
{
  memset(histogram, 0, COLORS_PER_COMPONENT * sizeof(int));

  for (gsize i = 0; i < image_length; i += COMPONENTS)
    histogram[pixelshop_image_calculate_luminance_from_pixel (&image[i])]++;
}

static void
pixelshop_image_calculate_min_and_max_luminance (PixelshopImage *self)
{
  guint8 i = 0;
  gboolean done = false;

  while (!done) {
    if (self->original_histogram[i] != 0)
      done = true;
    else
      i++;
  }
  self->min_luminance = i;

  i = 255;
  done = false;

  while (!done) {
    if (self->original_histogram[i] != 0)
      done = true;
    else
      i--;
  }

  self->max_luminance = i;
}

void
pixelshop_image_load_image (guint8 *contents, int length, PixelshopImage *self)
{
  int n;

  self->original_image = stbi_load_from_memory (contents, length, &self->original_width,
                                                &self->original_height, &n, COMPONENTS);

  self->original_stride = self->original_width * COMPONENTS;
  self->original_length = self->original_stride * self->original_height;

  self->scaled_image = g_malloc (self->original_length);
  self->edited_image = g_malloc (self->original_length);

  pixelshop_image_reset_edited_image_properties (self);
  pixelshop_image_reset_edited_image (self);

  pixelshop_image_calculate_histogram_from_colored_image (self->original_image,
                                                          self->original_length,
                                                          self->original_histogram);

  pixelshop_image_calculate_min_and_max_luminance (self);
}

static void
pixelshop_image_mirror_horizontally (PixelshopImage *self)
{
  guint8 pixel_buffer[COMPONENTS];
  gsize line_offset = 0;
  gsize line_end_offset;

  for (gsize i = 0; i < self->scaled_height; i++) {
    line_end_offset = line_offset + self->scaled_stride - COMPONENTS;

    for (gsize j = 0; j < self->scaled_stride / 2; j += COMPONENTS) {
      memcpy (pixel_buffer, &self->edited_image[line_offset + j], COMPONENTS);

      memcpy (&self->edited_image[line_offset + j],
              &self->edited_image[line_end_offset - j], COMPONENTS);

      memcpy (&self->edited_image[line_end_offset - j],
              pixel_buffer, COMPONENTS);
    }

    line_offset += self->scaled_stride;
  }
}

static void
pixelshop_image_mirror_vertically (PixelshopImage *self)
{
  guint8 line_buffer[self->scaled_stride];
  gsize line_offset = 0;
  gsize last_line_offset = self->scaled_length - self->scaled_stride;

  for (gsize i = 0; i < self->scaled_height / 2; i++) {
    memcpy (line_buffer, &self->edited_image[line_offset], self->scaled_stride);

    memcpy (&self->edited_image[line_offset],
            &self->edited_image[last_line_offset - line_offset],
            self->scaled_stride);

    memcpy (&self->edited_image[last_line_offset - line_offset],
            line_buffer, self->scaled_stride);

    line_offset += self->scaled_stride;
  }
}

void
pixelshop_image_toggle_mirrored_horizontally (PixelshopImage *self)
{
  self->mirrored_horizontally = !self->mirrored_horizontally;

  pixelshop_image_mirror_horizontally (self);
}

void
pixelshop_image_toggle_mirrored_vertically (PixelshopImage *self)
{
  self->mirrored_vertically = !self->mirrored_vertically;

  pixelshop_image_mirror_vertically (self);
}

static void
pixelshop_image_apply_mirror (PixelshopImage *self)
{
  if (self->mirrored_vertically)
    pixelshop_image_mirror_vertically(self);

  if (self->mirrored_horizontally)
    pixelshop_image_mirror_horizontally(self);
}

static void
pixelshop_image_apply_quantization (PixelshopImage *self)
{
  guint16 range_size = self->max_luminance - self->min_luminance + 1;

  if (self->quantization_colors < range_size) {

    gdouble bin_size = (gdouble)range_size / (gdouble)self->quantization_colors;

    guint8 map[256] = {0};

    gdouble end = self->min_luminance - 0.5 + bin_size;
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

    gsize line_offset = 0;

    for (gsize m = 0; m < self->scaled_height; m++) {

      for (gsize j = 0; j < self->scaled_stride; j += COMPONENTS) {
        memset (&self->edited_image[line_offset + j],
                map[self->edited_image[line_offset + j]], COMPONENTS);
      }

      line_offset += self->scaled_stride;
    }
  }
}

static void
pixelshop_image_apply_grayscale (PixelshopImage *self)
{
  guint8 lum;

  for (gsize i = 0; i < self->scaled_length; i += COMPONENTS) {
    lum = pixelshop_image_calculate_luminance_from_pixel (&self->edited_image[i]);
    memset (&self->edited_image[i], lum, COMPONENTS);
  }

  pixelshop_image_apply_quantization (self);
}

static void
pixelshop_image_revert_grayscale (PixelshopImage *self)
{
  pixelshop_image_reset_edited_image_from_scaled (self);
  pixelshop_image_apply_mirror(self);
}

void
pixelshop_image_toggle_grayscale (PixelshopImage *self)
{
  self->grayscale = !self->grayscale;

  if (self->grayscale)
    pixelshop_image_apply_grayscale(self);
  else
    pixelshop_image_revert_grayscale(self);
}

void
pixelshop_image_set_quantization_colors (int colors, PixelshopImage *self)
{
  self->quantization_colors = colors;

  pixelshop_image_revert_grayscale (self);
  pixelshop_image_apply_grayscale (self);
}

GdkTexture*
pixelshop_image_get_original_texture (PixelshopImage *self)
{
  GBytes *bytes = g_bytes_new (self->original_image, self->original_length);

  return gdk_memory_texture_new (self->original_width, self->original_height,
                                 MEMORY_FORMAT, bytes, self->original_stride);
}

GdkTexture*
pixelshop_image_get_edited_texture (PixelshopImage *self)
{
  GBytes *bytes = g_bytes_new (self->edited_image, self->scaled_length);

  return gdk_memory_texture_new (self->scaled_width, self->scaled_height,
                                 MEMORY_FORMAT, bytes, self->scaled_stride);
}

void
pixelshop_image_get_original_histogram (PixelshopImage *self, int *histogram)
{
  memcpy (histogram, self->original_histogram, COLORS_PER_COMPONENT * sizeof(int));
}

void
pixelshop_image_get_edited_histogram (PixelshopImage *self, int *histogram)
{
  if (self->grayscale) {
    pixelshop_image_calculate_histogram_from_grayscale_image (self->edited_image,
                                                              self->scaled_length,
                                                              histogram);
  }
  else {
    pixelshop_image_calculate_histogram_from_colored_image (self->edited_image,
                                                            self->scaled_length,
                                                            histogram);
  }
}

static void custom_stbi_write_mem(void *buf, void *data, int size)
{
  pixelshop_image_file_buffer *c = (pixelshop_image_file_buffer*)buf;
  char *dst = (char *)c->context;
  char *src = (char *)data;
  int cur_pos = c->last_pos;

  if (c->capacity < c->last_pos + size){
    c->capacity += FILE_BUFFER_CAPACITY_INCREMENT;
    c->context = realloc(c->context, c->capacity);
  }

  for (int i = 0; i < size; i++) {
    dst[cur_pos++] = src[i];
  }
  c->last_pos = cur_pos;
}

pixelshop_image_file_buffer
pixelshop_image_export_as_jpg (PixelshopImage *self)
{
  pixelshop_image_file_buffer file_buffer;
  file_buffer.capacity = DEFAULT_FILE_BUFFER_CAPACITY;
  file_buffer.last_pos = 0;
  file_buffer.context = malloc(file_buffer.capacity);
  stbi_write_jpg_to_func(custom_stbi_write_mem, &file_buffer, self->scaled_width,
                         self->scaled_height, COMPONENTS, self->edited_image, 100);

  return file_buffer;
}

static void
pixelshop_image_class_init (PixelshopImageClass *klass)
{

}

static void
pixelshop_image_init (PixelshopImage *self)
{
  self->quantization_colors = 256;
}

