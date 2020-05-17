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

static void
process_sound_samples(gint count)
	{
	Spectrum	*spectrum = gkrellmss->spectrum;
	SoundSample	*ss;
	gshort		l, r;
	glong		ll;
	gint		i, k, n_samples;

	gkrellmss->buf_count = count / sizeof(SoundSample);

	gkrellmss->left_value = 0;
	gkrellmss->right_value = 0;
	ss = gkrellmss->buffer;
	n_samples = spectrum->scale->n_samples;
	for (i = 0; i < gkrellmss->buf_count; ++i)
		{
		l = ss[i].left;
		r = ss[i].right;
		if (   gkrellmss->mode == SOUND_MODE_SPECTRUM
			&& spectrum->fftw_samples < n_samples
		   )
			{
			k = spectrum->fftw_samples++;
			ll = (l + r) / 2;
#if defined(HAVE_FFTW3)
			spectrum->fftw_data_in[k] = (double) ll / 32768.0;
#elif defined(HAVE_FFTW2)
			spectrum->fftw_data_in[k] = (fftw_real) ll / 32768.0;
#endif
			/* draw_spectrum() takes over when fftw_data_in[] is loaded */
			}
		l = abs(l);
		r = abs(r);
		if (l > gkrellmss->left_value)
			gkrellmss->left_value = l;
		if (r > gkrellmss->right_value)
			gkrellmss->right_value = r;
		}
	if (DEBUG_TEST())
		printf("  ** process_sound_samples[%d]: left=%d right=%d\n",
				count, gkrellmss->left_value, gkrellmss->right_value);

	/* Multiply by 1/sqrt(2) so VU meter krells approximate RMS voltage
	*/
	gkrellmss->left_value = gkrellmss->left_value * 707 / 1000;
	gkrellmss->right_value = gkrellmss->right_value * 707 / 1000;

	if (gkrellmss->oscope->x_append)
		{
		gkrellmss_oscope_trace(CHANNEL_LR);
		gkrellm_draw_chart_to_screen(gkrellmss->chart);
		}
	gkrellmss->streaming = TRUE;
	}

static void
reset_sound_data(void)
	{
	gkrellmss->fd = -1;
	gkrellmss->handle = NULL;
	gkrellmss->stream_open = FALSE;

	if (gkrellmss->input_id)
		gdk_input_remove(gkrellmss->input_id);
	gkrellmss->input_id = 0;

	gkrellmss->buf_count = 0;
	gkrellmss->buf_index = 0;
	gkrellmss->oscope->x_append = 0;
	gkrellmss->oscope->y_append = 0;
	}


static void
sdlib_NOP(void)
	{
	}


#if defined(HAVE_ALSA)
#include "sdlib-alsa.c"
#endif

#if defined(HAVE_ESOUND)
#include "sdlib-esd.c"
#endif

void
gkrellmss_add_sound_sources(void)
	{
	SoundSource	*source;

#if defined(HAVE_ALSA)
	gkrellmss_alsa_source_init();
#endif

#if defined(HAVE_ESOUND)
	gkrellmss_esd_source_init();
#endif

	/* An empty sound source so monitoring can be turned of.
	|  Need this for ALSA in case want to run another capture app.
	*/
	source = g_new0(SoundSource, 1);
	source->name = g_strdup(_("Off"));
	source->type = SOUND_NONE;
	source->open_stream = sdlib_NOP;
	source->close_stream = sdlib_NOP;
	/* A dummy device with no open/close */
	gkrellmss->sound_source_list =
			g_list_append(gkrellmss->sound_source_list, source);
	}
