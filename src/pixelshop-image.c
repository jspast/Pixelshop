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

  // Stores the edited image, with scale and filters applied
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
  int edited_height;
  int edited_width;
  gsize edited_stride;
  gsize edited_length;
  gboolean grayscale;

  // Normalized cumulative histogram of target image
  float target_histogram[COLORS_PER_COMPONENT];
};

G_DEFINE_FINAL_TYPE (PixelshopImage, pixelshop_image, G_TYPE_OBJECT)

static void
pixelshop_image_reset_edited_image_properties (PixelshopImage *self)
{
  self->edited_height = self->original_height;
  self->edited_width = self->original_width;
  self->edited_stride = self->original_stride;
  self->edited_length = self->original_length;

  self->grayscale = false;
}

void
pixelshop_image_reset_edited_image (PixelshopImage *self)
{
  g_free_sized (self->edited_image, self->edited_length);

  pixelshop_image_reset_edited_image_properties (self);

  self->edited_image = g_malloc (self->edited_length);

  memcpy (self->edited_image, self->original_image, self->edited_length);
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

  self->edited_image = g_malloc (self->original_length);

  pixelshop_image_reset_edited_image_properties (self);
  pixelshop_image_reset_edited_image (self);

  pixelshop_image_calculate_histogram_from_colored_image (self->original_image,
                                                          self->original_length,
                                                          self->original_histogram);

  pixelshop_image_calculate_min_and_max_luminance (self);
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
  GBytes *bytes = g_bytes_new (self->edited_image, self->edited_length);

  return gdk_memory_texture_new (self->edited_width, self->edited_height,
                                 MEMORY_FORMAT, bytes, self->edited_stride);
}

void
pixelshop_image_mirror_horizontally (PixelshopImage *self)
{
  guint8 pixel_buffer[COMPONENTS];
  gsize line_offset = 0;
  gsize line_end_offset;

  for (gsize i = 0; i < self->edited_height; i++) {
    line_end_offset = line_offset + self->edited_stride - COMPONENTS;

    for (gsize j = 0; j < self->edited_stride / 2; j += COMPONENTS) {
      memcpy (pixel_buffer, &self->edited_image[line_offset + j], COMPONENTS);

      memcpy (&self->edited_image[line_offset + j],
              &self->edited_image[line_end_offset - j], COMPONENTS);

      memcpy (&self->edited_image[line_end_offset - j],
              pixel_buffer, COMPONENTS);
    }

    line_offset += self->edited_stride;
  }
}

void
pixelshop_image_mirror_vertically (PixelshopImage *self)
{
  guint8 line_buffer[self->edited_stride];
  gsize line_offset = 0;
  gsize last_line_offset = self->edited_length - self->edited_stride;

  for (gsize i = 0; i < self->edited_height / 2; i++) {
    memcpy (line_buffer, &self->edited_image[line_offset], self->edited_stride);

    memcpy (&self->edited_image[line_offset],
            &self->edited_image[last_line_offset - line_offset],
            self->edited_stride);

    memcpy (&self->edited_image[last_line_offset - line_offset],
            line_buffer, self->edited_stride);

    line_offset += self->edited_stride;
  }
}

static void
pixelshop_image_apply_quantization (PixelshopImage *self, int levels)
{
  guint16 range_size = self->max_luminance - self->min_luminance + 1;

  if (levels < range_size) {

    gdouble bin_size = (gdouble)range_size / (gdouble)levels;

    guint8 map[256] = {0};

    gdouble end = self->min_luminance - 0.5 + bin_size;
    gdouble mid = end - bin_size / 2;

    guint16 k = 0;
    guint16 i;

    for (i = 0; i < levels && k < 256; i++) {

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

    for (gsize m = 0; m < self->edited_height; m++) {

      for (gsize j = 0; j < self->edited_stride; j += COMPONENTS) {
        memset (&self->edited_image[line_offset + j],
                map[self->edited_image[line_offset + j]], COMPONENTS);
      }

      line_offset += self->edited_stride;
    }
  }
}

static void
pixelshop_image_apply_grayscale (PixelshopImage *self)
{
  guint8 lum;

  for (gsize i = 0; i < self->edited_length; i += COMPONENTS) {
    lum = pixelshop_image_calculate_luminance_from_pixel (&self->edited_image[i]);
    memset (&self->edited_image[i], lum, COMPONENTS);
  }

  self->grayscale = true;
}

void
pixelshop_image_apply_quantized_grayscale (PixelshopImage *self, int levels)
{
  pixelshop_image_apply_grayscale (self);
  pixelshop_image_apply_quantization (self, levels);
}

void
pixelshop_image_apply_brightness (PixelshopImage *self, int brightness)
{
  for (gsize i = 0; i < self->edited_length; i++)
    self->edited_image[i] = MAX(MIN(self->edited_image[i] + brightness, 255), 0);
}

void
pixelshop_image_apply_contrast (PixelshopImage *self, float contrast)
{
  for (gsize i = 0; i < self->edited_length; i++)
    self->edited_image[i] = MAX(MIN(self->edited_image[i] * contrast, 255), 0);
}

void
pixelshop_image_apply_negative (PixelshopImage *self)
{
  for (gsize i = 0; i < self->edited_length; i++)
    self->edited_image[i] = MAX(MIN(255 - self->edited_image[i], 255), 0);
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
                                                              self->edited_length,
                                                              histogram);

  }
  else {
    pixelshop_image_calculate_histogram_from_colored_image (self->edited_image,
                                                            self->edited_length,
                                                            histogram);
  }
}

static void
pixelshop_image_compute_cumulative_histogram (int *hist, int *hist_cum)
{
  hist_cum[0] = hist[0];
  for (int i = 1; i < COLORS_PER_COMPONENT; i++)
      hist_cum[i] = hist_cum[i - 1] + hist[i];
}

static void
pixelshop_image_normalize_histogram_int (int *hist_src, int *hist_dest,
                                         int max_value, int pixels)
{
  for (int i = 0; i < COLORS_PER_COMPONENT; i++)
    hist_dest[i] = MIN((long long)hist_src[i] * max_value / pixels, max_value);
}

static void
pixelshop_image_normalize_histogram_float (int *hist_src, float *hist_dest,
                                           float max_value, int pixels)
{
  for (int i = 0; i < COLORS_PER_COMPONENT; i++)
    hist_dest[i] = MIN((float)hist_src[i] * max_value / (float)pixels, max_value);
}

static void
pixelshop_image_equalize_grayscale_image (PixelshopImage *self)
{
  int hist[COLORS_PER_COMPONENT];

  pixelshop_image_calculate_histogram_from_grayscale_image (self->edited_image,
                                                            self->edited_length,
                                                            hist);

  pixelshop_image_compute_cumulative_histogram (hist, hist);
  pixelshop_image_normalize_histogram_int (hist, hist, COLORS_PER_COMPONENT - 1,
                                           self->edited_width * self->edited_height);

  for (int i = 0; i < self->edited_length; i += COMPONENTS)
    memset (&self->edited_image[i], hist[self->edited_image[i]], COMPONENTS);
}

static void
pixelshop_image_equalize_colored_image (PixelshopImage *self)
{
  int hist[COLORS_PER_COMPONENT];

  pixelshop_image_calculate_histogram_from_colored_image (self->edited_image,
                                                          self->edited_length,
                                                          hist);

  pixelshop_image_compute_cumulative_histogram (hist, hist);

  pixelshop_image_normalize_histogram_int (hist, hist, COLORS_PER_COMPONENT - 1,
                                           self->edited_width * self->edited_height);

  for (int i = 0; i < self->edited_length; i++)
    self->edited_image[i] = hist[self->edited_image[i]];
}

void
pixelshop_image_equalize (PixelshopImage *self)
{
  if (self->grayscale)
    pixelshop_image_equalize_grayscale_image (self);

  else
    pixelshop_image_equalize_colored_image (self);
}

void
pixelshop_image_load_target_image (PixelshopImage *self, guint8 *contents, int length)
{
  int n, width, height;

  guint8 *image = stbi_load_from_memory (contents, length, &width,
                                         &height, &n, COMPONENTS);

  int image_length = width * height * COMPONENTS;
  int hist[COLORS_PER_COMPONENT];
  pixelshop_image_calculate_histogram_from_grayscale_image (image, image_length, hist);
  stbi_image_free (image);

  pixelshop_image_compute_cumulative_histogram (hist, hist);

  pixelshop_image_normalize_histogram_float (hist, self->target_histogram,
                                             1, width * height);
}

static int
pixelshop_image_find_target_shade_level (int shade_level, float *hist, float *target_hist)
{
  int i = 0;

  while (i < COLORS_PER_COMPONENT - 1 && target_hist[i] < hist[shade_level])
      i++;

  return i;
}

void
pixelshop_image_match (PixelshopImage *self)
{
  int hist_cum[COLORS_PER_COMPONENT];
  float hist_norm[COLORS_PER_COMPONENT];
  int hm[COLORS_PER_COMPONENT];

  pixelshop_image_get_edited_histogram (self, hist_cum);

  pixelshop_image_compute_cumulative_histogram (hist_cum, hist_cum);

  pixelshop_image_normalize_histogram_float (hist_cum, hist_norm, 1,
                                             self->edited_width * self->edited_height);

  for (int shade_level = 0; shade_level < COLORS_PER_COMPONENT; shade_level++) {
    hm[shade_level] = pixelshop_image_find_target_shade_level (shade_level, hist_norm,
                                                               self->target_histogram);
  }

  for (int i = 0; i < self->edited_length; i++)
    self->edited_image[i] = hm[self->edited_image[i]];
}

void
pixelshop_image_zoom_out (PixelshopImage *self, int x_factor, int y_factor)
{
  // Avoid computation if no change
  if (x_factor == 1 && y_factor == 1)
    return;

  // Avoid factor bigger than the dimension
  x_factor = MIN(x_factor, self->edited_width);
  y_factor = MIN(y_factor, self->edited_height);

  int new_width = (self->edited_width + x_factor - 1) / x_factor;
  int new_height = (self->edited_height + y_factor - 1) / y_factor;
  int new_stride = new_width * COMPONENTS;
  int new_length = new_stride * new_height;

  guint8 *new_image = g_malloc (new_length);

  for (int new_y = 0; new_y < new_height; new_y++) {
    for (int new_x = 0; new_x < new_width; new_x++) {
      int sum[COMPONENTS] = {0};
      int pixels = 0;

      for (int i = 0; i < y_factor; i++) {
        for (int j = 0; j < x_factor; j++) {
          int old_x = new_x * x_factor + j;
          int old_y = new_y * y_factor + i;

          if (old_x < self->edited_width && old_y < self->edited_height) {
            for (int k = 0; k < COMPONENTS; k++) {
              sum[k] += self->edited_image[old_y * self->edited_stride +
                                           old_x * COMPONENTS + k];
            }
            pixels++;
          }
        }
      }

      guint8 mean[COMPONENTS];
      for (int i = 0; i < COMPONENTS; i++)
        mean[i] = sum[i] / pixels;

      memcpy (&new_image[new_y * new_stride + new_x * COMPONENTS], mean, COMPONENTS);
    }
  }

  g_free_sized (self->edited_image, self->edited_length);
  self->edited_image = new_image;
  self->edited_width = new_width;
  self->edited_height = new_height;
  self->edited_stride = new_stride;
  self->edited_length = new_length;
}

// Factor is fixed 2x2
void
pixelshop_image_zoom_in (PixelshopImage *self)
{
  int new_width = self->edited_width * 2 - 1;
  int new_height = self->edited_height * 2 - 1;
  int new_stride = new_width * COMPONENTS;
  int new_length = new_stride * new_height;

  guint8 *new_image = g_malloc (new_length);

  // Copy pixels from old image
  for (int old_y = 0; old_y < self->edited_height; old_y++) {
    int new_y = old_y * 2;
    for (int old_x = 0; old_x < self->edited_width; old_x++) {
      int new_x = old_x * 2;

      memcpy (&new_image[new_y * new_stride + new_x * COMPONENTS],
              &self->edited_image[old_y * self->edited_stride + old_x * COMPONENTS],
              COMPONENTS);
    }
  }

  // Interpolate horizontally
  for (int new_y = 0; new_y < new_height; new_y += 2) {
    for (int new_x = 1; new_x < new_width; new_x += 2) {
      for (int i = 0; i < COMPONENTS; i++) {
        int sum = 0;
        sum += new_image[new_y * new_stride + (new_x - 1) * COMPONENTS + i];
        sum += new_image[new_y * new_stride + (new_x + 1) * COMPONENTS + i];
        new_image[new_y * new_stride + new_x * COMPONENTS + i] = sum / 2;
      }
    }
  }

  // Interpolate vertically
  for (int new_y = 1; new_y < new_height; new_y += 2) {
    for (int new_x = 0; new_x < new_width; new_x++) {
      for (int i = 0; i < COMPONENTS; i++) {
        int sum = 0;
        sum += new_image[(new_y - 1) * new_stride + new_x * COMPONENTS + i];
        sum += new_image[(new_y + 1) * new_stride + new_x * COMPONENTS + i];
        new_image[new_y * new_stride + new_x * COMPONENTS + i] = sum / 2;
      }
    }
  }

  g_free_sized (self->edited_image, self->edited_length);
  self->edited_image = new_image;
  self->edited_width = new_width;
  self->edited_height = new_height;
  self->edited_stride = new_stride;
  self->edited_length = new_length;
}

void
pixelshop_image_rotate_left (PixelshopImage *self)
{
  int new_width = self->edited_height;
  int new_stride = new_width * COMPONENTS;

  guint8 *new_image = g_malloc (self->edited_length);

  for (int old_y = 0; old_y < self->edited_height; old_y++) {
    for (int old_x = 0; old_x < self->edited_width; old_x++) {
      int new_x = old_y;
      int new_y = self->edited_width - 1 - old_x;

      int old_index = old_y * self->edited_stride + old_x * COMPONENTS;
      int new_index = new_y * new_stride + new_x * COMPONENTS;

      memcpy (&new_image[new_index], &self->edited_image[old_index], COMPONENTS);
    }
  }

  g_free_sized (self->edited_image, self->edited_length);
  self->edited_image = new_image;
  self->edited_height = self->edited_width;
  self->edited_width = new_width;
  self->edited_stride = new_stride;
}

void
pixelshop_image_rotate_right (PixelshopImage *self)
{
  int new_width = self->edited_height;
  int new_stride = new_width * COMPONENTS;

  guint8 *new_image = g_malloc (self->edited_length);

  for (int old_y = 0; old_y < self->edited_height; old_y++) {
    for (int old_x = 0; old_x < self->edited_width; old_x++) {
      int new_x = self->edited_height - 1 - old_y;
      int new_y = old_x;

      int old_index = old_y * self->edited_stride + old_x * COMPONENTS;
      int new_index = new_y * new_stride + new_x * COMPONENTS;

      memcpy (&new_image[new_index], &self->edited_image[old_index], COMPONENTS);
    }
  }

  g_free_sized (self->edited_image, self->edited_length);
  self->edited_image = new_image;
  self->edited_height = self->edited_width;
  self->edited_width = new_width;
  self->edited_stride = new_stride;
}

static void
pixelshop_image_apply_filter_colored (PixelshopImage *self, float *filter,
                                      int add_to_result)
{
  guint8 *new_image = g_malloc0 (self->edited_length);

  for (int y = 1; y < self->edited_height - 1; y++) {
    for (int x = 1; x < self->edited_width - 1; x++) {
      for (int i = 0; i < COMPONENTS; i++) {
        guint8 *dest = &new_image[y * self->edited_stride + x * COMPONENTS + i];

        float res;
        res = filter[8] * self->edited_image[(y - 1) * self->edited_stride + (x - 1) * COMPONENTS + i];
        res += filter[7] * self->edited_image[(y - 1) * self->edited_stride + x * COMPONENTS + i];
        res += filter[6] * self->edited_image[(y - 1) * self->edited_stride + (x + 1) * COMPONENTS + i];
        res += filter[5] * self->edited_image[y * self->edited_stride + (x - 1) * COMPONENTS + i];
        res += filter[4] * self->edited_image[y * self->edited_stride + x * COMPONENTS + i];
        res += filter[3] * self->edited_image[y * self->edited_stride + (x + 1) * COMPONENTS + i];
        res += filter[2] * self->edited_image[(y + 1) * self->edited_stride + (x - 1) * COMPONENTS + i];
        res += filter[1] * self->edited_image[(y + 1) * self->edited_stride + x * COMPONENTS + i];
        res += filter[0] * self->edited_image[(y + 1) * self->edited_stride + (x + 1) * COMPONENTS + i];
        res += add_to_result;
        res = fminf (res, 255);
        res = fmaxf (res, 0);

        memset (dest, res, 1);
      }
    }
  }

  g_free_sized (self->edited_image, self->edited_length);
  self->edited_image = new_image;
}

static void
pixelshop_image_apply_filter_grayscale (PixelshopImage *self, float *filter,
                                        int add_to_result)
{
  // Make source grayscale if not already
  if (!self->grayscale)
    pixelshop_image_apply_grayscale (self);

  guint8 *new_image = g_malloc0 (self->edited_length);

  for (int y = 1; y < self->edited_height - 1; y++) {
    for (int x = 1; x < self->edited_width - 1; x++) {
      guint8 *dest = &new_image[y * self->edited_stride + x * COMPONENTS];
      float res;

      res = filter[8] * self->edited_image[(y - 1) * self->edited_stride + (x - 1) * COMPONENTS];
      res += filter[7] * self->edited_image[(y - 1) * self->edited_stride + x * COMPONENTS];
      res += filter[6] * self->edited_image[(y - 1) * self->edited_stride + (x + 1) * COMPONENTS];
      res += filter[5] * self->edited_image[y * self->edited_stride + (x - 1) * COMPONENTS];
      res += filter[4] * self->edited_image[y * self->edited_stride + x * COMPONENTS];
      res += filter[3] * self->edited_image[y * self->edited_stride + (x + 1) * COMPONENTS];
      res += filter[2] * self->edited_image[(y + 1) * self->edited_stride + (x - 1) * COMPONENTS];
      res += filter[1] * self->edited_image[(y + 1) * self->edited_stride + x * COMPONENTS];
      res += filter[0] * self->edited_image[(y + 1) * self->edited_stride + (x + 1) * COMPONENTS];
      res += add_to_result;
      res = fminf (res, 255);
      res = fmaxf (res, 0);

      memset (dest, res, COMPONENTS);
    }
  }

  g_free_sized (self->edited_image, self->edited_length);
  self->edited_image = new_image;
}

void
pixelshop_image_apply_filter (PixelshopImage *self, float *filter,
                              gboolean colored, int add_to_result)
{
  if (colored)
    pixelshop_image_apply_filter_colored (self, filter, add_to_result);

  else
    pixelshop_image_apply_filter_grayscale (self, filter, add_to_result);
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
  stbi_write_jpg_to_func(custom_stbi_write_mem, &file_buffer, self->edited_width,
                         self->edited_height, COMPONENTS, self->edited_image, 100);

  return file_buffer;
}

static void
pixelshop_image_class_init (PixelshopImageClass *klass)
{

}

static void
pixelshop_image_init (PixelshopImage *self)
{

}

