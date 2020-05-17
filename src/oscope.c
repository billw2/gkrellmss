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

static Oscope	*oscope;



static void
trigger_delay(gint channel)
	{
	SoundSample	*ss = gkrellmss->buffer;
	gfloat		flimit;
	gint		y0, i, n, limit, trigger, delay;
	
	trigger = (gint) (oscope->vert_trigger * oscope->vert_max);
	delay = -2;
	flimit = oscope->samples_per_point;
	limit = flimit;
	for (i = 0; i < gkrellmss->buf_count - limit; )
		{
		for (y0 = 0, n = 0; n < limit; ++n)
			{
			if (channel == CHANNEL_L)
				y0 += ss[i].left;
			else if (channel == CHANNEL_R)
				y0 += ss[i].right;
			else if (channel == CHANNEL_LR)
				y0 += (ss[i].left + ss[i].right) / 2;
			}
		y0 = y0 / limit;
		if (y0 < trigger)
			delay = -1;
		if (y0 >= trigger && delay == -1)
			{
			delay = i;
			break;
			}
		flimit += oscope->samples_per_point;
		i = flimit;
		}
	gkrellmss->buf_index = (delay < 0) ? 0 : delay;
	}


static void
draw_oscope_line_trace(gint channel)
	{
	GkrellmChart	*chart	= gkrellmss->chart;
	SoundSample		*ss = gkrellmss->buffer;
	gfloat			flimit;
	gint			x, x0, y0, y1, i, limit, n;

	gdk_gc_set_foreground(gkrellmss->gc, gkrellm_in_color());

	y1 = oscope->y_append;
	x0 = oscope->x_append;
	oscope->x_append = 0;
	oscope->y_append = 0;
	flimit = gkrellmss->buf_index + oscope->samples_per_point;
	for (i = gkrellmss->buf_index, x = x0; x < chart->w;
				x += oscope->dx_per_point)
		{
		limit = flimit;
		if (limit >= gkrellmss->buf_count - 1)
			{
			oscope->y_append = y1;
			oscope->x_append = x;
			break;
			}
		for (n = 0, y0 = 0; i < limit; ++i, ++n)
			{
			if (channel == CHANNEL_L)
				y0 += ss[i].left;
			else if (channel == CHANNEL_R)
				y0 += ss[i].right;
			else if (channel == CHANNEL_LR)
				y0 += (ss[i].left + ss[i].right) / 2;
			}
		y0 /= n;
		y0 = (chart->h / 2) * -y0 / oscope->vert_max;
		y0 += chart->h / 2;
		if (x > 0)
			{
			gdk_draw_line(chart->pixmap, gkrellmss->gc,
					x - oscope->dx_per_point, y1, x, y0);
			}
		y1 = y0;
		flimit += oscope->samples_per_point;
		}
	gkrellmss->buf_index = 0;
	gkrellmss->buf_count = 0;
	}

static void
draw_oscope_bar_trace(gint channel)
	{
	GkrellmChart	*chart	= gkrellmss->chart;
	SoundSample		*ss = gkrellmss->buffer;
	gfloat			flimit;
	gint			x, x0, y, y0, y1, i, limit, n;
	gint			y0_prev, y1_prev;

	gdk_gc_set_foreground(gkrellmss->gc, gkrellm_in_color());

	y = y0 = y1 = 0;
	y0_prev = y1_prev = oscope->y_append;
	x0 = oscope->x_append;
	oscope->x_append = 0;
	oscope->y_append = 0;
	flimit = gkrellmss->buf_index + oscope->samples_per_point;
	for (i = gkrellmss->buf_index, x = x0; x < chart->w;
				x += oscope->dx_per_point)
		{
		limit = flimit;
		if (limit >= gkrellmss->buf_count - 1)
			{
			oscope->y_append = (y1_prev + y0_prev) / 2;
			oscope->x_append = x;
			break;
			}
		for (n = 0; i < limit; ++i, ++n)
			{
			if (channel == CHANNEL_L)
				y = ss[i].left;
			else if (channel == CHANNEL_R)
				y = ss[i].right;
			else if (channel == CHANNEL_LR)
				y = (ss[i].left + ss[i].right) / 2;
			else
				break;
			if (n == 0)
				y0 = y1 = y;
			else
				{
				if (y < y0)
					y0 = y;
				if (y > y1)
					y1 = y;
				}
			if (x > 0)
				{
				if (y0 > y1_prev)
					y0 = y1_prev;
				if (y1 < y0_prev)
					y1 = y0_prev;
				}
			}
		y0_prev = y0;
		y1_prev = y1;

		y0 = (chart->h / 2) * -y0 / oscope->vert_max;
		y1 = (chart->h / 2) * -y1 / oscope->vert_max;
		y0 += chart->h / 2;
		y1 += chart->h / 2;
		gdk_draw_line(chart->pixmap, gkrellmss->gc, x, y1, x, y0);
		flimit += oscope->samples_per_point;
		}
	gkrellmss->buf_index = 0;
	gkrellmss->buf_count = 0;
	}

static void
draw_oscope_grid(void)
	{
	GdkImage		*grid_image;
	GdkGC			*gc;
	GdkColor		color;
	GkrellmChart	*chart	= gkrellmss->chart;
	gint			w, h, x, dx;

	/* Draw grid lines on bg_src_pixmap so gkrellm_clear_chart_pixmap()
	|  will include grid lines.
	*/
	gkrellm_clean_bg_src_pixmap(chart);
	gkrellm_draw_chart_grid_line(chart, chart->bg_src_pixmap, chart->h / 4);
	gkrellm_draw_chart_grid_line(chart, chart->bg_src_pixmap, chart->h / 2);
	gkrellm_draw_chart_grid_line(chart, chart->bg_src_pixmap, 3 * chart->h/4);

	/* There is no vertical grid image, so I make a vertical grid line using
	|  pixel values from the horizontal grid image at desired x positions.
	*/
	gdk_drawable_get_size(chart->bg_grid_pixmap, &w, &h);
	grid_image = gdk_image_get(chart->bg_grid_pixmap, 0, 0, w, h);
	gc = gkrellm_draw_GC(3);
	dx = chart->w / HORIZONTAL_DIVS;
	for (x = dx; x < HORIZONTAL_DIVS * dx; x += dx)
		{
		color.pixel = gdk_image_get_pixel(grid_image, x, 0);
		gdk_gc_set_foreground(gc, &color);
		gdk_draw_line(chart->bg_src_pixmap, gc, x - 1, 0, x - 1, chart->h - 1);
		if (h > 1)
			{
			color.pixel = gdk_image_get_pixel(grid_image, x, 1);
			gdk_gc_set_foreground(gc, &color);
			gdk_draw_line(chart->bg_src_pixmap, gc, x, 0, x, chart->h - 1);
			}
		}
	gdk_image_destroy(grid_image);
	}

static void
draw_oscope_label_decals(void)
	{
	gchar	buf[32];

	if (oscope->usec_per_div >= 1000)
		sprintf(buf, "%d msec", oscope->usec_per_div / 1000);
	else
		sprintf(buf, "%d usec", oscope->usec_per_div);
	gkrellm_draw_decal_text(NULL, gkrellmss->label0_decal, buf, -1);
	}

static void
draw_oscope_labels(void)
	{
	GkrellmChart	*chart	= gkrellmss->chart;

	if (gkrellmss->show_tip)
		return;
	if (gkrellmss->stream_open && gkrellmss->extra_info)
		{
		draw_oscope_label_decals();
		gkrellm_draw_decal_on_chart(chart, gkrellmss->label0_decal, 2,
					chart->h - gkrellmss->label0_decal->h);
		}
	}


void
gkrellmss_oscope_horizontal_scaling(void)
	{
	GkrellmChart	*chart	= gkrellmss->chart;

	oscope->t_sample = 1 / (gfloat) SAMPLE_RATE;
	oscope->dx_per_point = 0;
	do
		{
		++oscope->dx_per_point;
		oscope->t_trace = (gfloat) oscope->usec_per_div * 1e-6;
		oscope->t_trace *= HORIZONTAL_DIVS;
		oscope->samples_per_point = oscope->t_trace / oscope->t_sample
					/ (gfloat) chart->w * (gfloat) oscope->dx_per_point;
		}
	while (oscope->samples_per_point < 1);
	}

void
gkrellmss_oscope_trace(gint channel)
	{
	if (oscope->dx_per_point > 1)
		draw_oscope_line_trace(channel);
	else
		draw_oscope_bar_trace(channel);
	}

void
gkrellmss_draw_oscope(gboolean force_reset, gboolean draw_grid)
	{
	GkrellmChart	*chart	= gkrellmss->chart;
	gint			y;

	if (draw_grid)
		draw_oscope_grid();
	if (!gkrellmss->streaming)
		{
		if (!oscope->reset || force_reset)
			{
			y = chart->h / 2;
			gkrellm_clear_chart_pixmap(chart);
			gdk_gc_set_foreground(gkrellmss->gc, gkrellm_in_color());
			gdk_draw_line(chart->pixmap, gkrellmss->gc, 0, y, chart->w - 1, y);
			draw_oscope_labels();
			}
		gkrellmss->buf_count = 0;
		gkrellmss->buf_index = 0;
		oscope->x_append = oscope->y_append = 0;
		oscope->reset = TRUE;
		return;
		}
	else if (!oscope->x_append && gkrellmss->buf_count)
		{
		gkrellm_clear_chart_pixmap(chart);	/* Draws the grid */
		trigger_delay(CHANNEL_LR);
		gkrellmss_oscope_trace(CHANNEL_LR);
		draw_oscope_labels();
		oscope->reset = FALSE;
		}
	}

Oscope *
gkrellmss_init_oscope(void)
	{
	oscope = g_new0(Oscope, 1);
	oscope->usec_per_div = 2000;
	oscope->vert_max = (gint) (gkrellmss->vert_sensitivity * 32767.0);
	return oscope;
	}
