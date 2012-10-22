
#include "bitmap_popup.h"

////////////////////////////////////////////////////////////////////////////////

typedef struct {

	GtkWidget *drawing_area;

	GtkAdjustment *adjustment;
	
	GdkPixbuf *pixbuf;
	GdkPixbuf *background;
	guint current_frame;
	guint frame_width;
	guint frame_height;
	guint frame_count;
	
	GtkWidget *menu;

} bitmap_popup;

static const gchar *bitmap_popup_key = "bitmap_popup";

////////////////////////////////////////////////////////////////////////////////

static gboolean bitmap_popup_destroy		( GtkWidget *widget, gpointer user_data );
static gboolean bitmap_popup_expose			( GtkWidget *wigdet, GdkEventExpose *event );
static gboolean bitmap_popup_button_press	( GtkWidget *wigdet, GdkEventButton *event );

static void		bitmap_popup_set_adjustment				( GtkWidget *widget, GtkAdjustment *adjustment );
static void		bitmap_popup_adjustment_changed			( GtkAdjustment *adjustment, gpointer data );
static void		bitmap_popup_adjustment_value_changed	( GtkAdjustment *adjustment, gpointer data );
static void		bitmap_popup_menuitem_activated			( GtkWidget *menu_item, gpointer data );

////////////////////////////////////////////////////////////////////////////////

GtkWidget *
bitmap_popup_new( GtkAdjustment *adjustment,
                 GdkPixbuf *pixbuf,
                 guint frame_width,
                 guint frame_height,
                 guint frame_count )
{
	bitmap_popup *self = g_malloc0 (sizeof(bitmap_popup));

	self->drawing_area = gtk_drawing_area_new ();
	self->pixbuf		= g_object_ref (pixbuf);
	self->frame_width	= frame_width;
	self->frame_height	= frame_height;
	self->frame_count	= frame_count;

	g_object_set_data (G_OBJECT (self->drawing_area), bitmap_popup_key, self);
	g_assert (g_object_get_data (G_OBJECT (self->drawing_area), bitmap_popup_key));

	g_signal_connect (G_OBJECT (self->drawing_area), "destroy", G_CALLBACK (bitmap_popup_destroy), NULL);
	
	g_signal_connect (G_OBJECT (self->drawing_area), "expose-event", G_CALLBACK (bitmap_popup_expose), NULL);

	g_signal_connect (G_OBJECT (self->drawing_area), "button-press-event", G_CALLBACK (bitmap_popup_button_press), NULL);
	
	gtk_widget_set_usize (self->drawing_area, frame_width, frame_height);
	
	// set up event mask
	gint event_mask = gtk_widget_get_events (self->drawing_area);
	event_mask |= GDK_BUTTON_PRESS_MASK;
	gtk_widget_set_events (self->drawing_area, event_mask);
	
	bitmap_popup_set_adjustment (self->drawing_area, adjustment);
	
	return self->drawing_area;
}

static gboolean
bitmap_popup_destroy( GtkWidget *widget, gpointer user_data )
{
	bitmap_popup *self = g_object_get_data (G_OBJECT (widget), bitmap_popup_key);
	bitmap_popup_set_bg (widget, NULL);
	g_free (self);
}

void bitmap_popup_set_bg (GtkWidget *widget, GdkPixbuf *pixbuf)
{
	bitmap_popup *self = g_object_get_data (G_OBJECT (widget), bitmap_popup_key);

	if (self->background) {
		g_object_unref (G_OBJECT (self->background));
	}

	self->background = pixbuf ? g_object_ref (G_OBJECT (pixbuf)) : NULL;

	gtk_widget_queue_draw (widget);
}

void bitmap_popup_set_strings (GtkWidget *widget, const gchar **strings)
{
	bitmap_popup *self = g_object_get_data (G_OBJECT (widget), bitmap_popup_key);

	g_assert (!self->menu);

	self->menu = gtk_menu_new ();
	
	gtk_menu_attach_to_widget (GTK_MENU (self->menu), widget, NULL);
	
	gint i;
	const gint min = gtk_adjustment_get_lower (self->adjustment);
	const gint max = gtk_adjustment_get_upper (self->adjustment);
	for (i = min; i <= max; i++)
	{
		gchar *label = g_strstrip (g_strdup(strings[i - min]));
		GtkWidget *menu_item = gtk_menu_item_new_with_label (label);
		gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
			(GtkSignalFunc) bitmap_popup_menuitem_activated,
			(gpointer) self );
		gtk_menu_append (self->menu, menu_item);
		g_object_unref (G_OBJECT (menu_item));
		g_free (label);
	}
	
	gtk_widget_show_all (self->menu);
}

////////////////////////////////////////////////////////////////////////////////

static gboolean
bitmap_popup_expose( GtkWidget *widget, GdkEventExpose *event )
{
	bitmap_popup *self = g_object_get_data (G_OBJECT (widget), bitmap_popup_key);
	
	if (self->background) {
		gdk_draw_pixbuf (
			widget->window,
			NULL,	// gc
			self->background,
			0,	// src_x
			0,	// src_y
			0,	// dest_x
			0,	// dest_y
			gdk_pixbuf_get_width (self->background),
			gdk_pixbuf_get_height (self->background),
			GDK_RGB_DITHER_NONE, 0, 0
		);	
	}
	
	gdk_draw_pixbuf (
		widget->window,
		NULL,	// gc
		self->pixbuf,
		0,	// src_x
		self->current_frame * self->frame_height,
		0,	// dest_x
		0,	// dest_y
		self->frame_width,
		self->frame_height,
		GDK_RGB_DITHER_NONE, 0, 0
	);
	
	return FALSE;
}

gboolean
bitmap_popup_button_press ( GtkWidget *widget, GdkEventButton *event )
{
	if (event->type == GDK_BUTTON_PRESS && event->button == 1)
	{
		bitmap_popup *self = g_object_get_data (G_OBJECT (widget), bitmap_popup_key);
		gtk_menu_popup (GTK_MENU (self->menu), NULL, NULL, NULL, NULL, event->button, event->time);
		return TRUE;
	}
	return FALSE;
}

void
bitmap_popup_menuitem_activated (GtkWidget *menu_item, gpointer data)
{
	bitmap_popup *self = data;
	
	GList *list = gtk_container_get_children (GTK_CONTAINER (self->menu));
	int i = g_list_index (list, menu_item);
	g_list_free (list);
	
	gdouble lower = gtk_adjustment_get_lower (self->adjustment);
	gtk_adjustment_set_value (self->adjustment, lower + i);
}

void
bitmap_popup_update (GtkWidget *widget)
{
	bitmap_popup *self = g_object_get_data (G_OBJECT (widget), bitmap_popup_key);
	
	gdouble value = gtk_adjustment_get_value (self->adjustment);
	gdouble lower = gtk_adjustment_get_lower (self->adjustment);
	gdouble upper = gtk_adjustment_get_upper (self->adjustment);
	guint	frame = self->frame_count * ((value - lower) / (upper - lower));

	self->current_frame = MIN (frame, (self->frame_count - 1));
	
	gtk_widget_queue_draw (widget);
}

void
bitmap_popup_adjustment_changed			( GtkAdjustment *adjustment, gpointer data )
{
	bitmap_popup_update (data);
}

void
bitmap_popup_adjustment_value_changed	( GtkAdjustment *adjustment, gpointer data )
{
	bitmap_popup_update (data);
}

void
bitmap_popup_set_adjustment( GtkWidget *widget, GtkAdjustment *adjustment )
{
	bitmap_popup *self = g_object_get_data (G_OBJECT (widget), bitmap_popup_key);

	if (self->adjustment)
	{
		gtk_signal_disconnect_by_data (GTK_OBJECT (self->adjustment), (gpointer) self);
		gtk_object_unref (GTK_OBJECT (self->adjustment) );
	}
	
	self->adjustment = g_object_ref (GTK_OBJECT (adjustment) );

	gtk_signal_connect (GTK_OBJECT (adjustment), "changed",
		(GtkSignalFunc) bitmap_popup_adjustment_changed,
		(gpointer) widget );
		
	gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		(GtkSignalFunc) bitmap_popup_adjustment_value_changed,
		(gpointer) widget );
	
	bitmap_popup_adjustment_changed (adjustment, widget);
}

////////////////////////////////////////////////////////////////////////////////

