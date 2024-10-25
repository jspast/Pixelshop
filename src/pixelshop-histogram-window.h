#pragma once

#include <adwaita.h>

#include "pixelshop-image.h"

G_BEGIN_DECLS

#define PIXELSHOP_TYPE_HISTOGRAM_WINDOW (pixelshop_histogram_window_get_type())

G_DECLARE_FINAL_TYPE (PixelshopHistogramWindow, pixelshop_histogram_window, PIXELSHOP, HISTOGRAM_WINDOW, AdwWindow)

void
pixelshop_histogram_window_set_data (PixelshopHistogramWindow *self, int histogram[COLORS_PER_COMPONENT]);

G_END_DECLS

