#include "m_editor.h"
#include "gui_compat.h"


static const char * const WINDOW_DATA_KEY = "data";

static gboolean draw_callback(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	GdkRGBA color;

	guint width = gtk_widget_get_allocated_width(widget);
	guint height = gtk_widget_get_allocated_height(widget);

	gtk_style_context_get_color(
		gtk_widget_get_style_context(widget),
		0,
		&color);
	gdk_cairo_set_source_rgba(cr, &color);

	cairo_arc(cr, width / 2.0, height / 2.0, MIN(width, height) / 2.0, 0, 2 * G_PI);
	cairo_fill(cr);

	return FALSE;
}

void create_editor_pane_window(Value *v, WndHandle parent)
{
	Ref *r = Value_ref(*v);

	GtkWidget *window = gtk_drawing_area_new();
	g_object_set_data(G_OBJECT(window), WINDOW_DATA_KEY, r);

	g_signal_connect(G_OBJECT(window), "draw", G_CALLBACK(draw_callback), NULL);

	gtk_container_add(GTK_CONTAINER(parent), window);
}
