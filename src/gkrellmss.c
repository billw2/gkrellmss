/* GKrellMSS - GKrellM Sound Scope
|  Copyright (C) 2002-2020 Bill Wilson
|
|  Author:  Bill Wilson    billw@gkrellm.net
|
|  This program is free software which I release under the GNU General Public
|  License. You may redistribute and/or modify this program under the terms
|  of that license as published by the Free Software Foundation; either
|  version 2 of the License, or (at your option) any later version.
|
|  This program is distributed in the hope that it will be useful,
|  but WITHOUT ANY WARRANTY; without even the implied warranty of
|  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
|  GNU General Public License for more details.
| 
|  To get a copy of the GNU General Puplic License, write to the Free Software
|  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "gkrellmss.h"

#include "../pixmaps/krell_vu.xpm"
#include "../pixmaps/krell_vu_peak.xpm"

#include "../pixmaps/button_sweep_dn.xpm"
#include "../pixmaps/button_sweep_up.xpm"

#include "../pixmaps/krell_sensitivity.xpm"

/* ======================================================================== */
#define DEFAULT_CHART_HEIGHT 30

SoundMonitor	*gkrellmss;

static GkrellmMonitor *mon_sound;
static GkrellmChartconfig *chart_config;

static gint			style_id;

#if !defined(GKRELLM_HAVE_THEME_SCALE)
static GdkPixmap	*button_sweep_up_pixmap,
					*button_sweep_dn_pixmap;
static GdkBitmap	*button_sweep_up_mask,
					*button_sweep_dn_mask;
#endif

static GkrellmDecal	*decal_sweep_up,
					*decal_sweep_dn;

static GkrellmDecal	*mode_decal_button;
static GkrellmDecal	*option_decal_button;


static GkrellmKrell	*krell_in_motion;

static gint			vu_meter_width;


static void
draw_mode_button(gboolean pressed)
	{
	gint	frame, x;

	x = gkrellm_chart_width() - mode_decal_button->w + 1;
	frame = pressed ? D_MISC_BUTTON_IN : D_MISC_BUTTON_OUT;
	gkrellm_draw_decal_pixmap(NULL, mode_decal_button, frame);
	gkrellm_draw_decal_on_chart(gkrellmss->chart, mode_decal_button, x, 0);
	}

static void
draw_option_button(gboolean pressed)
	{
	gint	frame, x;

	x = gkrellm_chart_width() - option_decal_button->w
				- mode_decal_button->w + 1;
	frame = pressed ? D_MISC_BUTTON_IN : D_MISC_BUTTON_OUT;
	gkrellm_draw_decal_pixmap(NULL, option_decal_button, frame);
	gkrellm_draw_decal_on_chart(gkrellmss->chart, option_decal_button, x, 0);
	}

void
gkrellmss_sound_chart_draw(gboolean force_reset, gboolean draw_grid)
	{
	if (gkrellmss->mode == SOUND_MODE_OSCOPE)
		gkrellmss_draw_oscope(force_reset, draw_grid);
	else if (gkrellmss->mode == SOUND_MODE_SPECTRUM)
		gkrellmss_draw_spectrum(force_reset, draw_grid);

	if (!gkrellmss->stream_open)
		{
		if (gkrellmss->sound_source->type == SOUND_NONE)
			gkrellm_draw_chart_text(gkrellmss->chart, DEFAULT_STYLE_ID,
						_("\\c\\fSound source:\\n\\cOff"));
		else
			gkrellm_draw_chart_text(gkrellmss->chart, DEFAULT_STYLE_ID,
						gkrellmss->server_error);
		}

	if (gkrellmss->mouse_in_chart)
		{
		draw_mode_button(gkrellmss->mode_button_pressed);
		draw_option_button(gkrellmss->option_button_pressed);
		if (gkrellmss->show_tip)
			{
			gkrellm_draw_decal_text(NULL, gkrellmss->label0_decal,
						gkrellmss->tip_string, -1);
			gkrellm_draw_decal_on_chart(gkrellmss->chart,
						gkrellmss->label0_decal, 2,
						gkrellmss->chart->h - gkrellmss->label0_decal->h - 1);
			}
		}
	gkrellm_draw_chart_to_screen(gkrellmss->chart);
	gkrellmss->streaming = FALSE;
	}

static gint
cb_chart_release(GtkWidget *widget, GdkEventButton *ev, gpointer data)
	{
	gkrellmss->mode_button_pressed = FALSE;
	gkrellmss->option_button_pressed = FALSE;
	return TRUE;
	}

static gint
cb_chart_press(GtkWidget *widget, GdkEventButton *ev, gpointer data)
	{
	Spectrum	*spectrum = gkrellmss->spectrum;

	if (gkrellm_in_decal(mode_decal_button, ev))
		{
		gkrellmss->mode += 1;
		if (gkrellmss->mode >= N_SOUND_MODES)
			gkrellmss->mode = 0;
		gkrellmss->mode_button_pressed = TRUE;
		gkrellm_config_modified();
		gkrellmss_sound_chart_draw(FORCE_RESET, DRAW_GRID);
		}
	else if (gkrellm_in_decal(option_decal_button, ev))
		{
		gkrellmss->option_button_pressed = TRUE;
		gkrellmss_option_menu(ev);
		}
	else if (gkrellmss->stream_open && ev->button == 1)
		{
		if (gkrellmss->mode == SOUND_MODE_SPECTRUM && !spectrum->reset)
			{
			if (spectrum->x_highlight > 0)
				spectrum->x_highlight = 0;
			else
				spectrum->x_highlight = gkrellmss->x_mouse;
			}
		}
	else if (ev->button == 2 || !gkrellmss->stream_open)
		{
		if (!gkrellmss->stream_open)
			{
			(*gkrellmss->sound_source->open_stream)();
			}
		else
			{
			gkrellmss->extra_info = !gkrellmss->extra_info;
			gkrellm_config_modified();
			gkrellmss_sound_chart_draw(FORCE_RESET, 0);
			}
		}
	else if (   ev->button == 3
			 || (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS)
			)
		gkrellm_chartconfig_window_create(gkrellmss->chart);
	return TRUE;
	}

static gboolean
cb_chart_enter(GtkWidget *w, GdkEventButton *ev, gpointer data)
	{
	gkrellmss->mouse_in_chart = TRUE;
	draw_mode_button(FALSE);
	draw_option_button(FALSE);
	gkrellm_draw_chart_to_screen(gkrellmss->chart);
	return TRUE;
	}

static gint
cb_chart_leave(GtkWidget *w, GdkEventButton *ev, gpointer data)
	{
	gkrellmss->mouse_in_chart = FALSE;
	gkrellmss->mode_button_pressed = FALSE;
	gkrellmss->option_button_pressed = FALSE;
	gkrellmss->show_tip = FALSE;
	gkrellm_decal_text_clear(gkrellmss->label0_decal);
	gkrellm_decal_text_clear(gkrellmss->label1_decal);
	gkrellmss_sound_chart_draw(FORCE_RESET, 0);
	return TRUE;
	}

static gint
cb_chart_motion(GtkWidget *widget, GdkEventButton *ev)
	{
	gint	last_show_tip = gkrellmss->show_tip;
	gchar	*last_string = gkrellmss->tip_string;

	gkrellmss->x_mouse = ev->x;

	gkrellmss->show_tip = TRUE;
	if (gkrellm_in_decal(mode_decal_button, ev))
		gkrellmss->tip_string = _("Display mode");
	else if (gkrellm_in_decal(option_decal_button, ev))
		gkrellmss->tip_string = _("Options menu");
	else
		gkrellmss->show_tip = FALSE;

	if (   last_show_tip != gkrellmss->show_tip
		|| (   gkrellmss->tip_string && last_string
			&& strcmp(gkrellmss->tip_string, last_string)
		   )
	   )
		gkrellmss_sound_chart_draw(FORCE_RESET, DRAW_GRID);
	return TRUE;
	}


static void
update_sound(void)
	{
	Oscope		*oscope = gkrellmss->oscope;
	gint		d, left, right, left_peak, right_peak;

	if ((left = gkrellmss->left_value) > oscope->vert_max)
		left = oscope->vert_max;
	if ((right = gkrellmss->right_value) > oscope->vert_max)
		right = oscope->vert_max;
	left_peak = gkrellmss->left_peak_value - oscope->vert_max / 30;
	if ((d = gkrellmss->left_peak_value - left) > 0)
		left_peak -= d / 30;
	right_peak = gkrellmss->right_peak_value - oscope->vert_max / 30;
	if ((d = gkrellmss->right_peak_value - right) > 0)
		right_peak -= d / 30;

	if (left_peak < left)
		left_peak = left;
	if (right_peak < right)
		right_peak = right;

	gkrellm_update_krell(gkrellmss->chart->panel, gkrellmss->krell_left, left);
	gkrellm_update_krell(gkrellmss->chart->panel, gkrellmss->krell_left_peak, left_peak);
	gkrellm_update_krell(gkrellmss->chart->panel, gkrellmss->krell_right, right);
	gkrellm_update_krell(gkrellmss->chart->panel, gkrellmss->krell_right_peak, right_peak);

	gkrellmss->left_peak_value = left_peak;
	gkrellmss->right_peak_value = right_peak;
	gkrellmss->left_value = 0;
	gkrellmss->right_value = 0;

	d = gkrellmss->krell_sensitivity_y_target - gkrellmss->krell_sensitivity_y;
	if (d > 0)
		{
		gkrellmss->krell_sensitivity_y += 1 + d / 4;
		gkrellm_move_krell_yoff(gkrellmss->chart->panel, gkrellmss->krell_sensitivity,
					gkrellmss->krell_sensitivity_y);
		}
	else if (d < 0)
		{
		gkrellmss->krell_sensitivity_y -= 1 - d / 4;
		gkrellm_move_krell_yoff(gkrellmss->chart->panel, gkrellmss->krell_sensitivity,
					gkrellmss->krell_sensitivity_y);
		}

	gkrellm_draw_panel_layers(gkrellmss->chart->panel);

	gkrellmss_sound_chart_draw(0, 0);
	gkrellmss->streaming = FALSE;
	}


static void
sound_vertical_scaling(void)
	{
	Oscope		*oscope = gkrellmss->oscope;
	Spectrum	*spectrum = gkrellmss->spectrum;
	gfloat		audio_taper;
	gint		spectrum_factor;

	audio_taper = log(1 + gkrellmss->vert_sensitivity) / log(2.0);

	oscope->vert_max = (gint) (audio_taper * 32767.0);

	spectrum_factor = spectrum->scale_index > 0 ? 24 : 16;
	spectrum->vert_max = (gint) (audio_taper * spectrum_factor);

	gkrellm_set_krell_full_scale(gkrellmss->krell_left, oscope->vert_max, 1);
	gkrellm_set_krell_full_scale(gkrellmss->krell_right, oscope->vert_max, 1);
	gkrellm_set_krell_full_scale(gkrellmss->krell_left_peak, oscope->vert_max, 1);
	gkrellm_set_krell_full_scale(gkrellmss->krell_right_peak, oscope->vert_max, 1);
	}

static void
update_slider_position(GkrellmKrell *k, gint x_ev)
	{
	gint	x,
			w = gkrellm_chart_width();

	if (x_ev < gkrellmss->vu_x0)
		x_ev = gkrellmss->vu_x0;
	if (x_ev >= w)
		x_ev = w - 1;
	gkrellmss->x_sensitivity_raw = x_ev;
	x = x_ev - gkrellmss->vu_x0;
	x = k->full_scale * x / (vu_meter_width - 1);
	if (x < 0)
		x = 0;

	gkrellmss->vert_sensitivity = (100.0 - (gfloat) x) / 100.0;
	if (gkrellmss->vert_sensitivity < 0.05)
		gkrellmss->vert_sensitivity = 0.05;
	if (gkrellmss->vert_sensitivity > 1.0)
		gkrellmss->vert_sensitivity = 1.0;

	if (DEBUG_INIT())
		printf("Vertical sensitivity: %f\n", gkrellmss->vert_sensitivity);

	sound_vertical_scaling();
	gkrellm_config_modified();
	gkrellm_update_krell(gkrellmss->chart->panel, k, (gulong) x);
	gkrellm_draw_panel_layers(gkrellmss->chart->panel);
	}


static gint
expose_event(GtkWidget *widget, GdkEventExpose *ev)
	{
	GdkPixmap	*pixmap	= NULL;

	if (widget == gkrellmss->chart->panel->drawing_area)
		pixmap = gkrellmss->chart->panel->pixmap;
	else if (widget == gkrellmss->chart->drawing_area)
		pixmap = gkrellmss->chart->pixmap;
	if (pixmap)
		gdk_draw_pixmap(widget->window, gkrellm_draw_GC(1), pixmap,
				ev->area.x, ev->area.y, ev->area.x, ev->area.y,
				ev->area.width, ev->area.height);
	return FALSE;
	}

static gint
cb_panel_enter(GtkWidget *widget, GdkEventButton *ev)
	{
	gkrellmss->krell_sensitivity_y_target = gkrellmss->krell_sensitivity_y_dn;
	return TRUE;
	}

static gint
cb_panel_leave(GtkWidget *widget, GdkEventButton *ev)
	{
	gkrellmss->krell_sensitivity_y_target = gkrellmss->krell_sensitivity_y_up;
	return TRUE;
	}

static gint
cb_panel_release(GtkWidget *widget, GdkEventButton *ev)
	{
	if (ev->button == 1)
		krell_in_motion = NULL;
	return TRUE;
	}

static gint
cb_panel_scroll(GtkWidget *widget, GdkEventScroll *ev)
	{
	gint	dx = 0;
	gint	delta;

	delta = vu_meter_width / 30;
	if (delta == 0)
		delta = 1;
	if (ev->direction == GDK_SCROLL_UP)
		dx = delta;
	else if (ev->direction == GDK_SCROLL_DOWN)
		dx = -delta;
	update_slider_position(gkrellmss->krell_sensitivity,
				gkrellmss->x_sensitivity_raw + dx);
	return TRUE;
	}

static gint
cb_panel_press(GtkWidget *widget, GdkEventButton *ev)
    {
	GkrellmKrell	*k = gkrellmss->krell_sensitivity;

	if (ev->button == 3)
		{
		gkrellm_open_config_window(mon_sound);
		return TRUE;
		}
	if (   ev->button == 1 && ev->x > gkrellmss->vu_x0
		&& ev->y >= k->y0 && ev->y <= k->y0 + k->h_frame)
		{
		krell_in_motion = k;
		update_slider_position(krell_in_motion, ev->x);
		}
	return TRUE;
	}

static gint
cb_panel_motion(GtkWidget *widget, GdkEventButton *ev)
	{
	GdkModifierType	state;

	if (krell_in_motion)
		{
		/* Check if button is still pressed, in case missed button_release
		*/
		state = ev->state;
		if (!(state & GDK_BUTTON1_MASK))
			{
			krell_in_motion = NULL;
			return TRUE;
			}
		update_slider_position(krell_in_motion, ev->x);
		}
	return TRUE;
	}


static void
height_changed(gpointer data)
	{
	if (gkrellmss->mode == SOUND_MODE_SPECTRUM)
		{
		gkrellmss_load_spectrum_images();
		}
	gkrellmss_sound_chart_draw(FORCE_RESET, DRAW_GRID);
	}

static void
create_chart(GtkWidget *vbox, gint first_create)
	{
	GkrellmChart	*cp = gkrellmss->chart;

	/* Create some decals that will be drawn on the chart.  They won't live
	|  in a panel where they can be automatically destroyed, so I must
    |  destroy them at create events and then recreate them with NULL panel
	|  and style pointers.  Make label0 full chart width so it can hold
	|  mode/option button "tooltip".
	*/
	gkrellm_destroy_decal(gkrellmss->label0_decal);
	gkrellm_destroy_decal(gkrellmss->label1_decal);
	gkrellmss->label0_decal = gkrellm_create_decal_text(NULL, "888 msec",
			gkrellm_chart_alt_textstyle(DEFAULT_STYLE_ID), NULL, 2, 0, -1);
	gkrellmss->label1_decal = gkrellm_create_decal_text(NULL, "8.8K",
			gkrellm_chart_alt_textstyle(DEFAULT_STYLE_ID), NULL, 2, 0, 0);

	gkrellm_destroy_decal(mode_decal_button);
	gkrellm_destroy_decal(option_decal_button);
	mode_decal_button = gkrellm_create_decal_pixmap(NULL,
			gkrellm_decal_misc_pixmap(), gkrellm_decal_misc_mask(),
			N_MISC_DECALS, NULL, 0, 0);
	option_decal_button = gkrellm_create_decal_pixmap(NULL,
			gkrellm_decal_misc_pixmap(), gkrellm_decal_misc_mask(),
			N_MISC_DECALS, NULL, 0, 0);

	gkrellm_set_chart_height_default(cp, DEFAULT_CHART_HEIGHT);
	gkrellm_chart_create(vbox, mon_sound, cp, &chart_config);

	gkrellm_set_chartconfig_flags(chart_config, NO_CONFIG_FIXED_GRIDS);

	/* The only config is height and I don't call gkrellm draw chart routines,
	|  so set the chart draw function to just reset the chart.  It will only
	|  be called when the chart height is changed.
	*/
	gkrellm_set_draw_chart_function(cp, height_changed, NULL);

	if (first_create)
		{
		gtk_signal_connect(GTK_OBJECT(cp->drawing_area), "expose_event",
				(GtkSignalFunc) expose_event, NULL);
		gtk_signal_connect(GTK_OBJECT(cp->drawing_area),
				"button_press_event", (GtkSignalFunc) cb_chart_press, cp);
		gtk_signal_connect(GTK_OBJECT(cp->drawing_area),
				"button_release_event", (GtkSignalFunc) cb_chart_release, cp);
		gtk_signal_connect(GTK_OBJECT(cp->drawing_area), "enter_notify_event",
				(GtkSignalFunc) cb_chart_enter, NULL);
		gtk_signal_connect(GTK_OBJECT(cp->drawing_area), "leave_notify_event",
				(GtkSignalFunc) cb_chart_leave, NULL);
		gtk_signal_connect(GTK_OBJECT(cp->drawing_area),
				"motion_notify_event", (GtkSignalFunc) cb_chart_motion, NULL);
		gtk_widget_show(vbox);
		}
	gkrellmss_oscope_horizontal_scaling();
	gkrellmss_load_spectrum_images();
	gkrellmss_sound_chart_draw(FORCE_RESET, DRAW_GRID);
	}

static void
cb_sweep_pressed(GkrellmDecalbutton *button)
	{
	Oscope		*oscope = gkrellmss->oscope;
	gint		dir;

	dir = GPOINTER_TO_INT(button->data);
	dir = (dir == 0) ? 1 : -1;
	if (gkrellmss->mode == SOUND_MODE_OSCOPE)
		{
		oscope->usec_per_div = gkrellm_125_sequence(oscope->usec_per_div + dir,
				TRUE, LOW_SWEEP, HIGH_SWEEP, FALSE, FALSE);
		gkrellmss_oscope_horizontal_scaling();
		}
	else if (gkrellmss->mode == SOUND_MODE_SPECTRUM)
		gkrellmss_change_spectrum_scale(dir);
	gkrellmss_sound_chart_draw(FORCE_RESET, 0);
	}

static void
create_panel_buttons(GkrellmPanel *p)
	{
	GkrellmPiximage	*im	= NULL;
	GkrellmStyle	*style;
	GkrellmMargin	*margin;
	gint			x, y;

	style = gkrellm_meter_style(style_id);
	margin = gkrellm_get_style_margins(style);

	y = 3;
	gkrellm_get_gkrellmrc_integer("sound_button_sweep_yoff", &y);

	if (!gkrellm_load_piximage("buttom_sweep_dn", NULL, &im, STYLE_NAME))
		gkrellm_load_piximage("button_sweep_dn", button_sweep_dn_xpm,
					&im, STYLE_NAME);

#if defined(GKRELLM_HAVE_THEME_SCALE)
	y *= gkrellm_get_theme_scale();
	decal_sweep_dn = gkrellm_make_scaled_decal_pixmap(p, im, style,
				2, margin->left, y, 0, 0);
#else
	gkrellm_scale_piximage_to_pixmap(im, &button_sweep_dn_pixmap,
				&button_sweep_dn_mask, 0, 0);
	decal_sweep_dn = gkrellm_create_decal_pixmap(p, button_sweep_dn_pixmap,
				button_sweep_dn_mask, 2, style, margin->left, y);
#endif
	gkrellm_make_decal_button(p, decal_sweep_dn, cb_sweep_pressed,
				GINT_TO_POINTER(0), 1, 0);

	x = decal_sweep_dn->x;
	y = decal_sweep_dn->y;

	if (!gkrellm_load_piximage("buttom_sweep_up", NULL, &im, STYLE_NAME))
		gkrellm_load_piximage("button_sweep_up", button_sweep_up_xpm,
					&im, STYLE_NAME);
#if defined(GKRELLM_HAVE_THEME_SCALE)
	decal_sweep_up = gkrellm_make_scaled_decal_pixmap(p, im, style,
				2, x + decal_sweep_dn->w, y, 0, 0);
#else
	gkrellm_scale_piximage_to_pixmap(im, &button_sweep_up_pixmap,
				&button_sweep_up_mask, 0, 0);
	decal_sweep_up = gkrellm_create_decal_pixmap(p, button_sweep_up_pixmap,
				button_sweep_up_mask, 2, style,
				x + decal_sweep_dn->w, y);
#endif
	gkrellm_make_decal_button(p, decal_sweep_up, cb_sweep_pressed,
				GINT_TO_POINTER(1), 1, 0);

	gkrellm_decal_on_top_layer(decal_sweep_dn, TRUE);
	gkrellm_decal_on_top_layer(decal_sweep_up, TRUE);

	gkrellmss->vu_x0 = decal_sweep_up->x + decal_sweep_up->w;
	vu_meter_width = gkrellm_chart_width() - gkrellmss->vu_x0;
	}

static GkrellmKrell *
default_or_themed_krell(GkrellmPanel *p, gchar *name, GkrellmPiximage *im,
		gint yoff, gint depth, gint x_hot, gint expand,
		gint left_margin, gint right_margin)
	{
	GkrellmKrell	*k;
	GkrellmStyle	*style;
	gint			y;

	/* Work with a copy since we may modify the style.
	*/
	style = gkrellm_copy_style(gkrellm_meter_style_by_name(name));
	if (!gkrellm_style_is_themed(style, 0))
		gkrellm_set_style_krell_values(style, yoff, depth, x_hot,
				expand, 1, left_margin, right_margin);
	else
		{
		if (style->krell_left_margin == 0)
			style->krell_left_margin = left_margin;
		}
	k = gkrellm_create_krell(p, im, style);
	gkrellm_monotonic_krell_values(k, FALSE);

	y = k->y0;
#if defined(GKRELLM_HAVE_THEME_SCALE)
	y *= gkrellm_get_theme_scale();
#endif
	gkrellm_move_krell_yoff(p, k, y);

	/* Unlike the style pointer passed to gkrellm_panel_configure(), the krells
	|  don't need the style to persist.
	*/
	g_free(style);
	return k;
	}

static void
create_panel(GtkWidget *vbox, gint first_create)
	{
	GkrellmPiximage	*im = NULL;
	GkrellmPanel	*p;
	gint			x;

	p = gkrellmss->chart->panel;
	create_panel_buttons(p);
	x = gkrellmss->vu_x0;

	gkrellm_load_piximage("krell_vu", krell_vu_xpm, &im, STYLE_NAME);
	gkrellmss->krell_left = default_or_themed_krell(p, "sound.vu_left",
				im, 3, 1, 59, KRELL_EXPAND_LEFT, x, 0);
	gkrellmss->krell_right = default_or_themed_krell(p, "sound.vu_right",
				im, 9, 1, 59, KRELL_EXPAND_LEFT, x, 0);

	gkrellm_load_piximage("krell_vu_peak", krell_vu_peak_xpm, &im, STYLE_NAME);
	gkrellmss->krell_left_peak = default_or_themed_krell(p, "sound.vu_left_peak",
				im, 2, 5, -1, KRELL_EXPAND_NONE, x, 0);
	gkrellmss->krell_right_peak = default_or_themed_krell(p, "sound.vu_right_peak",
				im, 8, 5, -1, KRELL_EXPAND_NONE, x, 0);

	sound_vertical_scaling();

	gkrellm_load_piximage("krell_sensitivity", krell_sensitivity_xpm,
				&im, STYLE_NAME);
	gkrellmss->krell_sensitivity = default_or_themed_krell(p, "sound.sensitivity",
				im, 0, 1, -1, KRELL_EXPAND_NONE, x, 0);
	gkrellm_set_krell_full_scale(gkrellmss->krell_sensitivity, 100, 1);

	gkrellmss->krell_sensitivity_y_up = -10;
	gkrellmss->krell_sensitivity_y_dn = 0;
	gkrellm_get_gkrellmrc_integer("sound_krell_sensitivity_y_up",
			&gkrellmss->krell_sensitivity_y_up);
	gkrellm_get_gkrellmrc_integer("sound_krell_sensitivity_y_dn",
			&gkrellmss->krell_sensitivity_y_dn);

#if defined(GKRELLM_HAVE_THEME_SCALE)
	gkrellmss->krell_sensitivity_y_up *= gkrellm_get_theme_scale();
	gkrellmss->krell_sensitivity_y_dn *= gkrellm_get_theme_scale();
#endif
	gkrellmss->krell_sensitivity_y_target = gkrellmss->krell_sensitivity_y_up;

	gkrellm_destroy_piximage(im);

	gkrellm_panel_configure(p, NULL, gkrellm_meter_style(style_id));
	gkrellm_panel_create(vbox, mon_sound, p);

	if (first_create)
		{
		g_signal_connect(G_OBJECT(p->drawing_area),
				"expose_event", G_CALLBACK(expose_event), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area),
				"button_press_event", G_CALLBACK(cb_panel_press), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area),
				"button_release_event", G_CALLBACK(cb_panel_release), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area),
				"scroll_event", G_CALLBACK(cb_panel_scroll), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area),
				"motion_notify_event", G_CALLBACK(cb_panel_motion), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area),
				"enter_notify_event", G_CALLBACK(cb_panel_enter), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area),
				"leave_notify_event", G_CALLBACK(cb_panel_leave), NULL);
		}
	gkrellm_update_krell(gkrellmss->chart->panel, gkrellmss->krell_sensitivity,
				(gulong) (100.0 * (1.0 - gkrellmss->vert_sensitivity)));
	gkrellmss->x_sensitivity_raw = (gint) ((1.0 - gkrellmss->vert_sensitivity)
				* (gfloat) vu_meter_width) + gkrellmss->vu_x0;

	/* To get the negative y_up, must move the krell since the create
	|  interprets negative offsets as special placement codes.
	*/
	gkrellm_move_krell_yoff(p, gkrellmss->krell_sensitivity,
				gkrellmss->krell_sensitivity_y_up);
	}

static void
gkrellmss_alloc_data(void)
	{
	if (!gkrellmss->buffer)
		{
		gkrellmss->buf_len = N_SAMPLES;
		gkrellmss->buffer = g_new0(SoundSample, gkrellmss->buf_len);
		}
	gkrellmss_spectrum_alloc_data();
	}

static void
create_sound(GtkWidget *vbox, gint first_create)
	{
	gkrellmss_alloc_data();
	if (first_create)
		{
		gkrellmss->chart = gkrellm_chart_new0();
		gkrellmss->chart->panel = gkrellm_panel_new0();
		(*gkrellmss->sound_source->open_stream)();
		}
	gkrellmss->gc = gkrellm_draw_GC(1);
	create_chart(vbox, first_create);
	create_panel(vbox, first_create);
#if defined(GKRELLM_CHECK_VERSION)
#if GKRELLM_CHECK_VERSION(2,1,0)
	gkrellm_spacers_set_types(mon_sound,
				GKRELLM_SPACER_CHART, GKRELLM_SPACER_METER);
#endif
#endif
	}


static void
save_config(FILE *f)
	{
	Oscope		*oscope = gkrellmss->oscope;
	Spectrum	*spectrum = gkrellmss->spectrum;
	SoundSource	*source;
	GList		*list;

	fprintf(f, "%s mode %d\n", CONFIG_KEYWORD, gkrellmss->mode);
	fprintf(f, "%s sensitivity %f\n", CONFIG_KEYWORD, gkrellmss->vert_sensitivity);
	fprintf(f, "%s extra_info %d\n", CONFIG_KEYWORD, gkrellmss->extra_info);
	fprintf(f, "%s usec_per_div %d\n", CONFIG_KEYWORD, oscope->usec_per_div);
	fprintf(f, "%s spectrum_scale %d\n", CONFIG_KEYWORD,spectrum->scale_index);
	fprintf(f, "%s sound_source %d\n", CONFIG_KEYWORD,
					gkrellmss->sound_source_index);
	gkrellm_save_chartconfig(f, chart_config, CONFIG_KEYWORD, NULL);

	for (list = gkrellmss->sound_source_list; list; list = list->next)
		{
		source = (SoundSource *) list->data;
		if (source->save_config)
			(*source->save_config)(f, CONFIG_KEYWORD);
		}
	}

static void
load_config(gchar *arg)
	{
	Oscope		*oscope = gkrellmss->oscope;
	Spectrum	*spectrum = gkrellmss->spectrum;
	SoundSource	*source;
	GList		*list;
	gchar		config[32], item[CFG_BUFSIZE];
	gint		n;

	n = sscanf(arg, "%31s %[^\n]", config, item);
	if (n != 2)
		return;
	if (!strcmp(config, "mode"))
		sscanf(item, "%d", &gkrellmss->mode);
	else if (!strcmp(config, "sensitivity"))
		{
		sscanf(item, "%f", &gkrellmss->vert_sensitivity);
		if (gkrellmss->vert_sensitivity < 0.05)
			gkrellmss->vert_sensitivity = 0.05;
		if (gkrellmss->vert_sensitivity > 1.0)
			gkrellmss->vert_sensitivity = 1.0;
		}
	else if (!strcmp(config, "extra_info"))
		sscanf(item, "%d", &gkrellmss->extra_info);
	else if (!strcmp(config, "usec_per_div"))
		sscanf(item, "%d", &oscope->usec_per_div);
	else if (!strcmp(config, "spectrum_scale"))
		sscanf(item, "%d", &spectrum->scale_index);
	else if (!strcmp(config, "sound_source"))
		{
		sscanf(item, "%d", &n);
		list = g_list_nth(gkrellmss->sound_source_list, n);
		if (!list)
			{
			list = gkrellmss->sound_source_list;
			n = 0;
			}
		gkrellmss->sound_source = (SoundSource *) list->data;
		gkrellmss->sound_source_index = n;
		}
	else if (!strcmp(config, GKRELLM_CHARTCONFIG_KEYWORD))
		gkrellm_load_chartconfig(&chart_config, item, 0);
	else
		{
		for (list = gkrellmss->sound_source_list; list; list = list->next)
			{
			source = (SoundSource *) list->data;
			if (source->load_config && !strcmp(config, source->name))
				(*source->load_config)(item);
			}
		}
	}

/* --------------------------------------------------------------------- */

static gchar	*info_text[] =
{
N_("<h>GKrellMSS - GKrellM Sound Scope\n"),
"\n",
N_("<h>VU Meter\n"),
N_("Two fast response krells show left and right audio signal levels\n"
	"averaged over about 1/20 sec.  There are also two slow decay peak\n"
	"response krells.\n"),
"\n",

N_("<h>Chart\n"),
N_("The chart has oscilloscope and spectrum analyzer modes toggled by\n"
	"pressing the mode button on the chart.\n"),
"\n",

N_("<h>Panel Buttons\n"),
N_("In oscilloscope mode, the two buttons to the left of the VU Meter select\n"
	"an oscope horizontal sweep speed ranging from 100 microseconds (usec)\n"
	"per division to 50 miliseconds (msec) per division.  There are 5\n"
	"horizontal divisions, so a trace sweep time can range from 500 usec\n"
	"(1/2000 sec) to 250 msec (1/4 sec).\n"),
"\n",
N_("In spectrum analyzer mode, the two buttons select a frequency resolution\n"
	"and range.  As the '>' button is pressed, there is better lower\n"
	"frequency resolution at the expense of slower update times.\n"),
"\n",
N_("<h>\nMouse Button Actions:\n"),
N_("<b>\tLeft "),
N_("click and drag in the VU Meter krell region to adjust VU Meter.\n"
	"\t\tand chart sensitivity.\n"),
N_("<b>\tLeft "),
N_("on the chart in spectrum analyzer mode toggles a frequency\n"
	"\t\tto draw highlighted.\n"),

N_("<b>\tWheel "),
N_("button anywhere in the panel also adjusts sensitivity.\n"),
N_("<b>\tMiddle "),
N_("on the chart toggles display of chart labels.\n"),
N_("<b>\tRight "),
N_("on the panel opens this configuration window.\n")

};

static void
create_tab(GtkWidget *tab_vbox)
	{
	GtkWidget	*tabs;
	GtkWidget	*vbox;
	GtkWidget	*text, *label;
	gchar		*buf;
	gint		i;

	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tab_vbox), tabs, TRUE, TRUE, 0);

#if 0
/* --Options Tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Options"));
#endif


/* --Info tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Info"));
	text = gkrellm_gtk_scrolled_text_view(vbox, NULL,
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	for (i = 0; i < sizeof(info_text)/sizeof(gchar *); ++i)
		gkrellm_gtk_text_view_append(text, _(info_text[i]));

/* --About tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("About"));
	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	buf = g_strdup_printf(_("GKrellMSS %d.%d %s\nGKrellM Sound Scope\n\n"
				"Copyright (c) 2002-2020 by Bill Wilson\n"
				"billw@gkrellm.net\n"
				"http://gkrellm.net\n\n"
				"Released under the GNU Public License"),
				GKRELLMSS_VERSION_MAJOR, GKRELLMSS_VERSION_MINOR,
				GKRELLMSS_EXTRAVERSION);
	label = gtk_label_new(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	g_free(buf);

	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	}


static GkrellmMonitor	monitor_sound =
	{
	N_(CONFIG_NAME),	/* Name, for config tab.	*/
	0,					/* Id,  0 if a plugin		*/
	create_sound,		/* The create function		*/
	update_sound,		/* The update function		*/
	create_tab,			/* The config tab create function	*/
	NULL,				/* Everthing is instant apply */

	save_config,		/* Save user conifg			*/
	load_config,		/* Load user config			*/
	CONFIG_KEYWORD,		/* config keyword			*/

	NULL,				/* Undef 2	*/
	NULL,				/* Undef 1	*/
	NULL,				/* Undef 0	*/

	MON_APM,			/* insert_before_id - place plugin before this mon */

	NULL,				/* Handle if a plugin, filled in by GKrellM		*/
	NULL				/* path if a plugin, filled in by GKrellM		*/
	};

GkrellmMonitor *
gkrellm_init_plugin(void)
	{
	gchar	*dummy_utf8 = NULL;

#ifdef ENABLE_NLS
	bind_textdomain_codeset("gkrellm-gkrellmss", "UTF-8");
#endif

	monitor_sound.name = _(monitor_sound.name);
	mon_sound = &monitor_sound;

	gkrellmss = g_new0(SoundMonitor, 1);
	gkrellmss->x_sensitivity_raw = gkrellm_chart_width() / 2;
	gkrellmss->extra_info = TRUE;
	gkrellmss->vert_sensitivity = 0.5;

	gkrellmss_add_sound_sources();
	if (!gkrellmss->sound_source_list)
		return NULL;
	gkrellmss_option_menu_build();

	gkrellmss->sound_source =
			(SoundSource *) gkrellmss->sound_source_list->data;
	gkrellmss->sound_source_index = 0;

	gkrellmss->oscope = gkrellmss_init_oscope();

	gkrellmss->spectrum = gkrellmss_init_spectrum();

	style_id = gkrellm_add_meter_style(mon_sound, STYLE_NAME);

	gkrellm_locale_dup_string(&dummy_utf8,
			_("\\cSound error?\\n\\f\\cClick here to\\n\\f\\ctry to open"),
			&gkrellmss->server_error);
	g_free(dummy_utf8);

	return &monitor_sound;
	}

