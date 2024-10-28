#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define PIXELSHOP_TYPE_IMAGE (pixelshop_image_get_type())

G_DECLARE_FINAL_TYPE (PixelshopImage, pixelshop_image, PIXELSHOP, IMAGE, GObject)

#define GRAYSCALE_R 0.299
#define GRAYSCALE_G 0.587
#define GRAYSCALE_B 0.144

#define COMPONENTS 3
#define BITS_PER_COMPONENT 8
#define COLORS_PER_COMPONENT 256
#define BITS_PER_PIXEL (COMPONENTS * BITS_PER_COMPONENT)

#define MEMORY_FORMAT GDK_MEMORY_R8G8B8

#define DEFAULT_FILE_BUFFER_CAPACITY 1000000
#define FILE_BUFFER_CAPACITY_INCREMENT 1000000

void
pixelshop_image_load_image (guint8 *contents, int length, PixelshopImage *self);

GdkTexture*
pixelshop_image_get_original_texture (PixelshopImage *self);

GdkTexture*
pixelshop_image_get_edited_texture (PixelshopImage *self);

void
pixelshop_image_reset_edited_image (PixelshopImage *self);

void
pixelshop_image_mirror_horizontally (PixelshopImage *self);

void
pixelshop_image_mirror_vertically (PixelshopImage *self);

void
pixelshop_image_apply_quantized_grayscale (PixelshopImage *self, int levels);

void
pixelshop_image_apply_brightness (PixelshopImage *self, int brightness);

void
pixelshop_image_apply_contrast (PixelshopImage *self, float contrast);

void
pixelshop_image_apply_negative (PixelshopImage *self);

void
pixelshop_image_get_original_histogram (PixelshopImage *self, int *histogram);

void
pixelshop_image_get_edited_histogram (PixelshopImage *self, int *histogram);

void
pixelshop_image_equalize (PixelshopImage *self);

void
pixelshop_image_load_target_image (PixelshopImage *self, guint8 *contents, int length);

void
pixelshop_image_match (PixelshopImage *self);

void
pixelshop_image_zoom_out (PixelshopImage *self, int x_factor, int y_factor);

void
pixelshop_image_zoom_in (PixelshopImage *self);

void
pixelshop_image_rotate_left (PixelshopImage *self);

void
pixelshop_image_rotate_right (PixelshopImage *self);

void
pixelshop_image_apply_filter (PixelshopImage *self, float *filter,
                              gboolean colored, int add_to_result);

typedef struct {
  int capacity;
  int last_pos;
  void *context;
} pixelshop_image_file_buffer;

pixelshop_image_file_buffer
pixelshop_image_export_as_jpg (PixelshopImage *self);

G_END_DECLS
