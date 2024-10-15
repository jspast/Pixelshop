#pragma once

#include "pixelshop-window.h"

#include <adwaita.h>

G_BEGIN_DECLS

#define PIXELSHOP_TYPE_IMAGE (pixelshop_image_get_type())

G_DECLARE_FINAL_TYPE (PixelshopImage, pixelshop_image, PIXELSHOP, IMAGE, GObject)

void
pixelshop_image_load_image (guint8 *contents, int length, PixelshopImage *self);

GdkTexture*
pixelshop_image_get_original_texture (PixelshopImage *self);

GdkTexture*
pixelshop_image_get_edited_texture (PixelshopImage *self);

void
pixelshop_image_reset_edits (PixelshopImage *self);

void
pixelshop_image_mirror_horizontally (PixelshopImage *self);

void
pixelshop_image_mirror_vertically (PixelshopImage *self);

void
pixelshop_image_apply_grayscale (PixelshopImage *self);

void
pixelshop_image_revert_grayscale (PixelshopImage *self);

void
pixelshop_image_toggle_mirrored_horizontally (PixelshopImage *self);

void
pixelshop_image_toggle_mirrored_vertically (PixelshopImage *self);

void
pixelshop_image_toggle_grayscale (PixelshopImage *self);

void
pixelshop_image_set_quantization_colors (int colors, PixelshopImage *self);

G_END_DECLS
