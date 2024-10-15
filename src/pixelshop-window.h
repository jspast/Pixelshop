#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define PIXELSHOP_TYPE_WINDOW (pixelshop_window_get_type())

G_DECLARE_FINAL_TYPE (PixelshopWindow, pixelshop_window, PIXELSHOP, WINDOW, AdwApplicationWindow)

G_END_DECLS
