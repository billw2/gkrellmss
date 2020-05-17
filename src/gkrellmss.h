/* GKrellM Sound Scope
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

#define PACKAGE "gkrellm-gkrellmss"

#include <gkrellm2/gkrellm.h>
#include <math.h>
#include "configure.h"

#if defined(HAVE_FFTW3)
#include <fftw3.h>
#elif defined(HAVE_FFTW2)
#include <rfftw.h>
#endif

#if  !defined(GKRELLM_VERSION_MAJOR) || (GKRELLM_VERSION_MAJOR<2)
#error This GKrellMSS plugin requires GKrellM version >= 2.0.0
#endif


#define	GKRELLMSS_VERSION_MAJOR	2
#define	GKRELLMSS_VERSION_MINOR	7
#define	GKRELLMSS_EXTRAVERSION ""

#define CONFIG_NAME     "Sound Scope"
#define CONFIG_KEYWORD  "sound"
#define STYLE_NAME      "sound"

#define	DEBUG()			(gkrellm_plugin_debug() == 45)
#define	DEBUG_INIT()	(gkrellm_plugin_debug() == 46)
#define	DEBUG_TEST()	(gkrellm_plugin_debug() == 47)

#define	SAMPLE_RATE		44100
#define	N_SAMPLES		1024
#define N_FFT_SAMPLES	8192

#define	HORIZONTAL_DIVS	5
#define	VERTICAL_DIVS	4

#define	CHANNEL_L	0
#define	CHANNEL_R	1
#define	CHANNEL_LR	2


typedef struct
	{
	short		left,
				right;
	}
SoundSample;


#define	SOUND_MODE_OSCOPE	0
#define	SOUND_MODE_SPECTRUM	1
#define N_SOUND_MODES		2



typedef struct
	{
	gint	start_bar,
			x0_chart,
			x0_bar,
			dx_bar;

	gint	*freq_array;
	gint	array_size;
	double	freq_quantum;

	gint	n_samples;
	fftw_plan *plan;
	}
	SpectrumScale;


typedef struct
	{
	gint		fftw_samples;
#if defined(HAVE_FFTW2)
	fftw_real	*fftw_data_in,
				*fftw_data_out,
				*fftw_power_spectrum;
#elif defined(HAVE_FFTW3)
	double		*fftw_data_in,
				*fftw_data_out,
				*fftw_power_spectrum;
#endif
	GdkPixmap	*bar,
				*bar_light;
	gint		scale_index;
	SpectrumScale *scale;
	gint		vert_max;
	gint		freq_highlighted,
				x_highlight;
	gboolean	reset;
	}
	Spectrum;


typedef struct
	{
	gint		usec_per_div,
				vert_max,
				dx_per_point;
	gboolean	dirty,
				reset;
	gfloat		vert_trigger;
	gfloat		t_sample,
				t_trace,
				samples_per_point;
	gint		x_append,
				y_append;
	}
Oscope;


#define	SOUND_NONE		1
#define	SOUND_CARD		2
#define SOUND_SERVER	3

typedef struct
	{
	gchar	*name;
	gint	type;
	gchar	*preselect_item;

	void	(*open_stream)();	
	void	(*close_stream)();	
	void	(*option_menu)();	
	void	(*load_config)();
	void	(*save_config)();
	}
	SoundSource;


typedef struct
	{
	Oscope		*oscope;
	Spectrum	*spectrum;
	gint		mode;
	GdkGC		*gc;
	GList		*sound_source_list;
	SoundSource	*sound_source;
	gint		sound_source_index;

	GkrellmChart *chart;
	GkrellmDecal *label0_decal,
				*label1_decal;

	GkrellmKrell *krell_left_peak,
				*krell_right_peak,
				*krell_left,
				*krell_right,
				*krell_sensitivity;

	gint		left_value,
				right_value,
				left_peak_value,
				right_peak_value;

	gint		vu_x0,
				x_sensitivity_raw,
				krell_sensitivity_y,
				krell_sensitivity_y_target,
				krell_sensitivity_y_up,
				krell_sensitivity_y_dn;
	gfloat		vert_sensitivity;

	gint		fd;
	gpointer	handle;
	gint		input_id;
	gboolean	stream_open,
				streaming,
				extra_info,
				mouse_in_chart,
				mode_button_pressed,
				option_button_pressed,
				show_tip;
	gchar		*tip_string;

	gint		x_mouse;
	gchar		*server_error;

	gint		buf_count;
	gint		buf_len;
	gint		buf_index;
	SoundSample *buffer;
	}
	SoundMonitor;


#define LOW_SWEEP   100
#define HIGH_SWEEP  50000

#define	FORCE_RESET	1
#define	DRAW_GRID	1


SoundMonitor	*gkrellmss;


void		gkrellmss_add_sound_sources(void);
void		gkrellmss_sound_chart_draw(gboolean reset, gboolean draw_grid);

/* oscope */
void		gkrellmss_draw_oscope(gboolean, gboolean);
void		gkrellmss_oscope_horizontal_scaling(void);
void		gkrellmss_oscope_trace(gint);
Oscope		*gkrellmss_init_oscope(void);

/* Spectrum analyzer */
void		gkrellmss_draw_spectrum(gboolean, gboolean);
void		gkrellmss_load_spectrum_images(void);
void		gkrellmss_change_spectrum_scale(gint);
void		gkrellmss_spectrum_alloc_data(void);
Spectrum	*gkrellmss_init_spectrum(void);


void		gkrellmss_option_menu(GdkEventButton *ev);
void		gkrellmss_option_menu_build(void);
