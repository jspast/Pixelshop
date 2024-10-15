#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define PIXELSHOP_TYPE_APPLICATION (pixelshop_application_get_type())

G_DECLARE_FINAL_TYPE (PixelshopApplication, pixelshop_application, PIXELSHOP, APPLICATION, AdwApplication)

PixelshopApplication *pixelshop_application_new (const char        *application_id,
                                                 GApplicationFlags  flags);

G_END_DECLS
