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

#include "../pixmaps/spectrum_bar.xpm"
#include "../pixmaps/spectrum_bar_light.xpm"

static Spectrum			*spectrum;

static fftw_plan	plan_1024, plan_2048, plan_4096, plan_8192;

static gint		debug_trigger;


  /* Process halfcomplex double data output from executing the r2r fft:
  |  	r[0],r[1],r2,...,r[n/2},i[(n+1)/2 - 1],...,i[2],i[1]
  |
  |  r[k] and i[k] are real and imaginary part of kth output.  So for an
  |  output array size N, the kth real part is output[k] and the kth
  |  imaginary part is output[N-k], except for k == 0 (and k == N/2 if
  |  N is even).
  */
static void
process_fftw_data(void)
	{
#if defined(HAVE_FFTW3)
	double		*out, *ps;
#else
	fftw_real	*out, *ps;
#endif
	gint		k, N;

	out = spectrum->fftw_data_out;
	ps = spectrum->fftw_power_spectrum;
	N = spectrum->fftw_samples;

	ps[0] = out[0] * out[0];		/* r[0] * r[0] DC component */
	for (k = 1; k < (N+1)/2; ++k)	/* (k < N/2 rounded up) */
		{
		ps[k] = out[k] * out[k] + out[N-k] * out[N-k];	/* Nyquist freq. */
		}
	}

static void
draw_spectrum_grid(void)
	{
	GdkImage		*grid_image;
	GdkGC			*gc;
	GdkColor		color;
	GkrellmChart	*chart	= gkrellmss->chart;
	SpectrumScale	*scale;
	gint			*freq_table;
	gint			k, w, h, x;

	scale = spectrum->scale;
	freq_table = &(scale->freq_array[0]);
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
	for (k = scale->start_bar; k < scale->array_size - 1; ++k)
		{
		x = freq_table[k];
		if (x != 100 && x != 1000 && x != 10000)
			continue;
		x = scale->x0_chart + (k-1) * scale->dx_bar + (scale->dx_bar - 1) / 2;
		color.pixel = gdk_image_get_pixel(grid_image, x, 0);
		gdk_gc_set_foreground(gc, &color);
		gdk_draw_line(chart->bg_src_pixmap, gc, x, 0, x, chart->h - 1);
		if (h > 1)
			{
			color.pixel = gdk_image_get_pixel(grid_image, x, 1);
			gdk_gc_set_foreground(gc, &color);
			gdk_draw_line(chart->bg_src_pixmap, gc, x+1, 0, x+1, chart->h - 1);
			}
		}
	gdk_image_destroy(grid_image);
	}

  /* Make frequency labels look nicer by some rounding off.  This is OK since
  |  each freq is really a frequency band and I'll round off so the change
  |  is a small fraction of the bandwidth.
  */
static void
draw_spectrum_decal_label(GkrellmDecal *d, gfloat f, gint x, gint y)
	{
	GkrellmChart	*chart	= gkrellmss->chart;
	gchar			buf[32];
	gint			n;

	n = (gint) f;
	if (f >= 15000)
		sprintf(buf, "%.0fK", f / 1000);
	else if (f > 10000)
		{
		/* round off to nearest 500 K */
		n = ((n + 250) / 500) * 500;
		f = (gfloat) n;
		sprintf(buf, "%.1fK", f / 1000);
		}
	else if (f >= 1500)
		sprintf(buf, "%.1fK", f / 1000);
	else if (f > 1000)
		{
		/* round off to nearest 50 */
		n = ((n + 25) / 50) * 50;
		f = (gfloat) n;
		sprintf(buf, "%.2fK", f / 1000);
		}
	else if (f >= 300)
		{
		/* round off to nearest 10 */
		n = ((n + 5) / 10) * 10;
		f = (gfloat) n;
		sprintf(buf, "%.0f", f);
		}
	else if (f >= 110)
		{
		/* round off to nearest 5 */
		n = ((n + 2) / 5) * 5;
		f = (gfloat) n;
		sprintf(buf, "%.0f", f);
		}
	else
		sprintf(buf, "%.0f", f);

	gkrellm_draw_decal_text(NULL, d, buf, -1);
	gkrellm_draw_decal_on_chart(chart, d, x, y);
	}

static void
draw_spectrum_labels(void)
	{
	SpectrumScale	*scale = spectrum->scale;
	gfloat			freq0 = 0, freq1 = 0;

	if (gkrellmss->show_tip)
		return;
	if (gkrellmss->stream_open)
		{
		if (spectrum->freq_highlighted > 0)
			freq0 = spectrum->freq_highlighted;
		else if (gkrellmss->extra_info)
			{
			freq0 = scale->freq_array[scale->start_bar];
			freq1 = scale->freq_array[scale->array_size - 2];
			}
		}
	if (freq0 > 0)
		draw_spectrum_decal_label(gkrellmss->label0_decal, freq0, 1, 1);
	if (freq1 > 0)
		draw_spectrum_decal_label(gkrellmss->label1_decal, freq1,
			gkrellm_chart_width() - gkrellmss->label1_decal->w, 1);
	}


static gboolean
set_bar_frequency(SpectrumScale *scale, gint *k, double *flog, double dflog)
	{
	gint	l, ftest;
	double	fl, fr, qhits;

	ftest = (gint)(exp(*flog + dflog) + 0.5);
	l = scale->freq_array[*k - 1];
	if (ftest > SAMPLE_RATE / 2)
		ftest = SAMPLE_RATE / 2;
	fl = exp((log(ftest) + log(l)) / 2.0);
	fr = exp(log(ftest) + dflog / 2.0);

	/* If there will be a freq_quantum sample point between fl and fr, ftest
	|  can have data to display.
	*/
	qhits = fr / scale->freq_quantum - fl / scale->freq_quantum;

	if (DEBUG() && ftest < 100)  /* look at small ftest band or lotsa output */
//	if (DEBUG() && ftest > 15000)
		printf(
"bar[%d-%s] l=%d ftest=%d fl=%.1f fr=%.1f hits=%.1f freq_quantum=%.1f\n",
			*k, (qhits > 1.0) ? "yes" : " no",
			l, ftest, fl, fr, qhits, scale->freq_quantum);

	if (   *k < scale->array_size - 1
		&& qhits > 1.0
	   )
		{
		scale->freq_array[*k] = ftest;
		*k += 1;
		*flog += dflog;
		return TRUE;
		}
	else
		*flog += dflog;
	return FALSE;
	}

  /* Construct the frequency table around power of 10 frequencies. So 100, 1k,
  |  and 10K bars will be on the chart.
  |  Fixme so f_min_10 is calculated and handle case for no f_min_10.
  */
static void
load_freq_array(SpectrumScale *scale, gint f_min, gint f_max, gint f_min_10,
		gint x0_bar, gint dx_bar, gint sample_size)
	{
	double	d, flog, dflog;
	gint	freq, i, k, w, n_bars, n_target;

	scale->n_samples = sample_size;
	scale->freq_quantum = (double) SAMPLE_RATE / (double) sample_size;
	if (sample_size == 8192)
		scale->plan = &plan_8192;
	else if (sample_size == 4096)
		scale->plan = &plan_4096;
	else if (sample_size == 2048)
		scale->plan = &plan_2048;
	else
		scale->plan = &plan_1024;

	w = gkrellm_chart_width();
	scale->start_bar = 1;
	scale->x0_bar = x0_bar;
	scale->dx_bar = dx_bar;

	n_bars = ((w > 120) ? 120 : w) / dx_bar;
	scale->array_size = n_bars + 2;
	g_free(scale->freq_array);
	scale->freq_array = g_new0(gint, scale->array_size);

	dflog = (log(f_max) - log(f_min)) / (double) (n_bars - 1);

	/* Try to put the right number of bars to left of f_min_10.  There will
	|  be less bars than the target if freq_quantum size prevents a target
	|  bar from getting any data.
	*/
	d = (log(f_min_10) - log(f_min)) / dflog;
	n_target = (gint) (d + 0.5);
	flog = log(f_min) - dflog;
	scale->freq_array[0] = exp(flog);
	for (k = 1, i = 0; i < n_target; ++i)
		set_bar_frequency(scale, &k, &flog, dflog);
	for (freq = f_min_10; 10 * freq < f_max; freq *= 10)
		{
		d = (log(10 * freq) - log(freq)) / dflog;
		n_target = (gint) (d + 0.5);
		flog = log(freq);
		scale->freq_array[k++] = freq;
		for (i = 0; i < n_target - 1; ++i)
			set_bar_frequency(scale, &k, &flog, dflog);
		}
	d = (log(f_max) - log(freq)) / dflog;
	n_target = (gint) (d + 0.5);
	flog = log(freq);
	scale->freq_array[k++] = freq;
	for (i = 0; i < n_target; ++i)
		set_bar_frequency(scale, &k, &flog, dflog);

	if (k < scale->array_size)
		scale->freq_array[k++] = exp(flog + dflog);
	scale->array_size = k;	/* Since may not be using all of freq_array */

	scale->x0_chart = (w - (scale->array_size - 2) * dx_bar) / 2;
	if (scale->x0_chart < 0)
		scale->x0_chart = 0;

	if (DEBUG())
		{
		printf("freq_array: n_bars=%d k=%d q=%.1f x0=%d\n",
				n_bars, k, scale->freq_quantum, scale->x0_chart);
		for (i = 0; i < scale->array_size; ++ i)
			printf("%d ", scale->freq_array[i]);
		printf("\n");
		}
	}



void
gkrellmss_draw_spectrum(gboolean force_reset, gboolean draw_grid)
	{
	GkrellmChart	*chart	= gkrellmss->chart;
	SpectrumScale	*scale;
	gint			*freq_table;
	gint			n, k, N, i;
#if defined(HAVE_FFTW3)
	double			*ps;
#else
	fftw_real		*ps;
#endif
	double			m, f, flimit;
	gint			x, y, dy;
	gboolean		highlight;

	if (draw_grid)
		draw_spectrum_grid();
	if (!gkrellmss->streaming)
		{
		if (!spectrum->reset || force_reset)
			{
			gkrellm_clear_chart_pixmap(chart);
			spectrum->freq_highlighted = 0;
			draw_spectrum_labels();
			}
		gkrellmss->buf_count = 0;
		gkrellmss->buf_index = 0;
		spectrum->reset = TRUE;
		return;
		}
	scale = spectrum->scale;
	if (spectrum->fftw_samples != scale->n_samples)
		return;

#if defined(HAVE_FFTW3)
	fftw_execute(*(scale->plan));
#elif defined(HAVE_FFTW2)
	rfftw_one(*(scale->plan), spectrum->fftw_data_in, spectrum->fftw_data_out);
#endif

	process_fftw_data();

	gkrellm_clear_chart_pixmap(chart);
	m = 0;
	n = 0;
	spectrum->freq_highlighted = 0;
	f = scale->freq_quantum;
	freq_table = &(scale->freq_array[0]);
	ps = spectrum->fftw_power_spectrum;
	N = scale->n_samples;
	flimit = exp((log(freq_table[0]) + log(freq_table[1])) / 2);

	if (DEBUG() && ++debug_trigger == 1)
		printf("n_samples=%d quanta=%f fstart=%f\n",
				N, scale->freq_quantum, flimit);

	for (k = 1; k < (N+1)/2; ++k, f += scale->freq_quantum)
		if (f > flimit)
			break;
	i = scale->start_bar;
	flimit = exp((log(freq_table[i]) + log(freq_table[i+1])) / 2);

	if (DEBUG() && debug_trigger == 1)
		printf("kstart=%d i=%d flimit=%f\n", k, i, flimit);

	while (i < scale->array_size - 1)
		{
		if (f < flimit && k < (N+1)/2)
			{
			m += ps[k++];
			++n;
			f += scale->freq_quantum;
			}
		else
			{
			if (DEBUG() && debug_trigger == 1)
				printf("drawing(%d) f=%.1f limit=%.1f k=%d n=%d m=%.1f\n",
				freq_table[i], f - scale->freq_quantum, flimit, k, n, m);

			x = scale->x0_chart + (i-1) * scale->dx_bar;
			highlight = FALSE;
			if (spectrum->x_highlight > 0)
				{
				if (   x > spectrum->x_highlight - scale->dx_bar
					&& x <= spectrum->x_highlight
				   )
					{
					spectrum->freq_highlighted = freq_table[i];
					highlight = TRUE;		
					}
				}
			else if (	gkrellmss->mouse_in_chart
					 &&	x > gkrellmss->x_mouse - scale->dx_bar
					 &&	x <= gkrellmss->x_mouse
					)
				{
				spectrum->freq_highlighted = freq_table[i];
				highlight = TRUE;		
				}
			if (n > 0)
				{
				m = sqrt(m) / (scale->n_samples / 200);
				dy = m * chart->h / spectrum->vert_max;
				if (dy > chart->h)
					dy = chart->h;
				y = chart->h - dy;
				if (dy > 0)
					{
					if (highlight)
						gdk_draw_pixmap(chart->pixmap, gkrellmss->gc,
							spectrum->bar_light, scale->x0_bar, y, x, y,
							scale->dx_bar, dy);
					else
						gdk_draw_pixmap(chart->pixmap, gkrellmss->gc,
							spectrum->bar, scale->x0_bar, y, x, y,
							scale->dx_bar, dy);
					}
				}
			n = 0;
			m = 0;
			++i;
			flimit = exp((log(freq_table[i]) + log(freq_table[i+1])) / 2);
			}
		}
	spectrum->fftw_samples = 0;
	spectrum->reset = FALSE;
	draw_spectrum_labels();
	}


static SpectrumScale scale_table[5];

#define N_SPECTRUM_SCALES (sizeof(scale_table) / sizeof(SpectrumScale))

void
gkrellmss_load_spectrum_images(void)
	{
	GkrellmChart	*chart	= gkrellmss->chart;
	GkrellmPiximage	*im = NULL;
	gint			w, dy;
	static gint		last_w;

	dy = chart->h;
	w = gkrellm_chart_width();
	if (w != last_w)
		{
		/* The min/max freq values are slightly tuned for a given sample size
		|  to give good spacings at the low end.  If min freq was much
		|  greater than freq_quantum, tuning would not be necessary.
		*/
		/* Fast response, so visually nicer:
		*/
		load_freq_array(&scale_table[0], 20, 25000, 100, 0, 2, 1024);
		load_freq_array(&scale_table[1], 20, 25000, 100, 0, 2, 2048);

		/* Better and more interesting low frequency data, but slower:
		*/
		load_freq_array(&scale_table[2], 22, 20000, 100, 0, 2, 4096);
		load_freq_array(&scale_table[3], 18, 20000, 100, 2, 1, 8192);
		load_freq_array(&scale_table[4], 10, 3000, 100, 0, 2, 8192);
		}
	last_w = w;
	gkrellm_load_piximage("spectrum_bar", spectrum_bar_xpm, &im, STYLE_NAME);
	gkrellm_scale_piximage_to_pixmap(im, &spectrum->bar, NULL, 3, dy);
	gkrellm_load_piximage("spectrum_bar_light", spectrum_bar_light_xpm,
			&im, STYLE_NAME);
	gkrellm_scale_piximage_to_pixmap(im, &spectrum->bar_light, NULL, 3, dy);
	spectrum->scale = &scale_table[spectrum->scale_index];
	}

void
gkrellmss_change_spectrum_scale(gint dir)
	{
	gint	old_index = spectrum->scale_index;

	if (dir > 0 && spectrum->scale_index > 0)
		spectrum->scale = &scale_table[--(spectrum->scale_index)];
	else if (dir < 0 && spectrum->scale_index < N_SPECTRUM_SCALES - 1)
		spectrum->scale = &scale_table[++(spectrum->scale_index)];
	if (spectrum->scale_index != old_index)
		{
		spectrum->x_highlight = 0;
		draw_spectrum_grid();
		gkrellm_config_modified();
		}
	spectrum->fftw_samples = 0;
	debug_trigger = 0;
	}

void
gkrellmss_spectrum_alloc_data(void)
	{
	if (spectrum->fftw_data_in)
		return;

	spectrum->scale = &scale_table[0];

#if defined(HAVE_FFTW3)
	spectrum->fftw_data_in =
				(double *) fftw_malloc(N_FFT_SAMPLES * sizeof(double));
	spectrum->fftw_data_out =
				(double *) fftw_malloc(N_FFT_SAMPLES * sizeof(double));
	spectrum->fftw_power_spectrum =
				(double *) fftw_malloc(N_FFT_SAMPLES * sizeof(double));

	/* Use fftw3 real to real transform of kind FFTW_R2HC which takes a real
	|  double array as input and outputs a double array of the same size.
	|  This computes a real input DFT with output in "halfcomplex" format:
	|
	|      r[0],r[1],r2,...,r[n/2},i[(n+1)/2 - 1],...,i[2],i[1]
	|
	|  So r[0] is the DC component.  See process_fftw_data().
	*/
	plan_1024 = fftw_plan_r2r_1d(1024, spectrum->fftw_data_in,
					spectrum->fftw_data_out, FFTW_R2HC, 0);
	plan_2048 = fftw_plan_r2r_1d(2048, spectrum->fftw_data_in,
					spectrum->fftw_data_out, FFTW_R2HC, 0);
	plan_4096 = fftw_plan_r2r_1d(4096, spectrum->fftw_data_in,
					spectrum->fftw_data_out, FFTW_R2HC, 0);
	plan_8192 = fftw_plan_r2r_1d(8192, spectrum->fftw_data_in,
					spectrum->fftw_data_out, FFTW_R2HC, 0);

	/* Ok, no more plans because N_FFT_SAMPLES is 8192 */

#elif defined(HAVE_FFTW2)
	spectrum->fftw_data_in = g_new0(fftw_real, N_FFT_SAMPLES);
	spectrum->fftw_data_out = g_new0(fftw_real, N_FFT_SAMPLES);
	spectrum->fftw_power_spectrum = g_new0(fftw_real, N_FFT_SAMPLES);

	plan_1024 = rfftw_create_plan(1024, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);
	plan_2048 = rfftw_create_plan(2048, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);
	plan_4096 = rfftw_create_plan(4096, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);
	plan_8192 = rfftw_create_plan(8192, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);
#endif

	}

Spectrum *
gkrellmss_init_spectrum(void)
	{
	spectrum = g_new0(Spectrum, 1);
	spectrum->vert_max = (gint) (gkrellmss->vert_sensitivity * 32.0);
	return spectrum;
	}
