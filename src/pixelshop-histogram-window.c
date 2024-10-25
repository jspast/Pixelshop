#include "pixelshop-histogram-window.h"
#include "pixelshop-image.h"

#include "include/gtkchart.h"

struct _PixelshopHistogramWindow
{
  AdwWindow parent_instance;

  AdwToolbarView *toolbar_view;

  GtkChart *histogram;
};

G_DEFINE_FINAL_TYPE (PixelshopHistogramWindow, pixelshop_histogram_window, ADW_TYPE_WINDOW)

void
pixelshop_histogram_window_set_data (PixelshopHistogramWindow *self, int histogram[COLORS_PER_COMPONENT])
{
  int max = 0;
  int occur;

  for (int i = 0; i < COLORS_PER_COMPONENT; i++){
    occur = histogram[i];

    gtk_chart_plot_point(self->histogram, i, occur);

    if (occur > max)
      max = occur;
  }

  gtk_chart_set_y_max(self->histogram, max);
}

static void
pixelshop_histogram_window_class_init (PixelshopHistogramWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/jspast/Pixelshop/pixelshop-histogram-window.ui");

  gtk_widget_class_bind_template_child (widget_class, PixelshopHistogramWindow, toolbar_view);
}

static void
pixelshop_histogram_window_init (PixelshopHistogramWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->histogram = GTK_CHART(gtk_chart_new());
  gtk_chart_set_type(self->histogram, GTK_CHART_TYPE_HISTOGRAM);
  gtk_chart_set_label(self->histogram, "Label");
  gtk_chart_set_x_max(self->histogram, 255);

  adw_toolbar_view_set_content (self->toolbar_view, GTK_WIDGET(self->histogram));
}

