#include "pixelshop-image.h"

struct _PixelshopImage
{
  PixelshopWindow parent_instance;

  gboolean mirrored_vertically;
  gboolean mirrored_horizontally;
  gboolean grayscale;
  guint16 quantization_colors;
  guint8 min_color;
  guint8 max_color;

  GdkTexture *original_texture;

  GByteArray *edited_buffer;
  gsize buffer_height;
  gsize buffer_width;
  gsize buffer_stride;
};

G_DEFINE_FINAL_TYPE (PixelshopImage, pixelshop_image, ADW_TYPE_APPLICATION_WINDOW)

void
pixelshop_image_load_image (GFile* *file)
{
  self->original_texture = gdk_texture_new_from_file (file, NULL);
  self->buffer_height = gdk_texture_get_height(self->original_texture);
  self->buffer_width = gdk_texture_get_width(self->original_texture);
}

static void
pixelshop_image_class_init (PixelshopImageClass *klass)
{

}

static void
pixelshop_application_init (PixelshopApplication *self)
{

}

