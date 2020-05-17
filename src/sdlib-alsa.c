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

#include <alsa/asoundlib.h>

#include <errno.h>

/* Name of the PCM device, like plughw:0,0  The first number is the
|  soundcard number and the second is the device number.
|  Using hw:0 would be for lower level graphics card access.
*/
static gchar	*alsa_device;

/* Digital audio stream data in alsa is a circular buffer of frames recorded
|  in consecutive groups of time periods.  When a time period is filled, the
|  snd_pcm_wait() call returns indicating data from the period may be read.
|  So ALSA periods must be at least 2 (we can empty one while the other fills).
*/
#define ALSA_PERIODS_MIN     2


static gboolean			alsa_thread_busy;
static gboolean			alsa_thread_abort;

static snd_pcm_uframes_t alsa_n_frames;

static gint		alsa_fd_pipe[2] = {-1, -1};

static gchar	*alsa_pipe_buf;


static gboolean
gss_alsa_real_close(void)
	{
	static gboolean		closing;

	if (closing)
		return FALSE;
	closing = TRUE;
	alsa_thread_abort = TRUE;
	while (alsa_thread_busy)
		usleep(1000);
	if (gkrellmss->handle)
		snd_pcm_close((snd_pcm_t *) gkrellmss->handle);
	if (alsa_fd_pipe[0] >= 0)
		close(alsa_fd_pipe[0]);
	if (alsa_fd_pipe[1] >= 0)
		close(alsa_fd_pipe[1]);
	alsa_fd_pipe[0] = alsa_fd_pipe[1] = -1;

	reset_sound_data();
	closing = FALSE;
	return TRUE;
	}

static void
gss_alsa_close_stream(void)
	{
	gss_alsa_real_close();
	}

  /* Read pipe output and process.
  */
static void
gss_alsa_input_read(gpointer data, gint fd, GdkInputCondition condition)
	{
	gint	nbytes;

	nbytes = read(fd, gkrellmss->buffer,
				gkrellmss->buf_len * sizeof(SoundSample));
	if (nbytes > 0)
		process_sound_samples(nbytes);
    else
        gss_alsa_real_close();
	}

  /* Read from the sound card via ALSA and write to gkrellmss via pipe.
  */
static void
gss_alsa_sound_card_read(void)
	{
	snd_pcm_t	*pcm;
	gint		ret, nbytes, max_bytes, err, n;
	gint		readi_count;

	pcm = (snd_pcm_t *) gkrellmss->handle;
//	snd_pcm_wait(pcm, 200);
	max_bytes = N_SAMPLES * sizeof(SoundSample);
	while (!alsa_thread_abort)
		{
		if (DEBUG_TEST())
			printf("\nCalling ALSA snd_pcm_readi().\n");

		readi_count = 0;
		while ((ret = snd_pcm_readi(pcm, alsa_pipe_buf, alsa_n_frames)) < 0)
			{
			if (ret == -EAGAIN)
				{
				usleep(20000);
				++readi_count;
				continue;
				}
			else
				{
				if (ret == -EPIPE)
					{
					if (DEBUG_TEST())
						printf("    -> calling snd_pcm_prepare()\n");
					err = snd_pcm_prepare(pcm);
					}
				else if (ret == -ESTRPIPE)
					{
					if (DEBUG_TEST())
						printf("    -> calling snd_pcm_resume()\n");
					while ((err = snd_pcm_resume(pcm)) == -EAGAIN)
						{
						if (DEBUG_TEST())
							printf("        EAGAIN, sleeping ...\n");
						sleep(1);
						}
					if (err < 0)
						err = snd_pcm_prepare(pcm);
					}
				else
					alsa_thread_abort = TRUE;
				}
			}
		nbytes = ret * sizeof(SoundSample);

		if (DEBUG_TEST())
			printf("snd_pcm_readi() [usleep %d] returned %d => nbytes=%d\n",
						readi_count, ret, nbytes);
		while (!alsa_thread_abort && nbytes > max_bytes)
			{
			if (DEBUG_TEST())
				printf("pipe write %d byte\n", nbytes);
			write(alsa_fd_pipe[1], alsa_pipe_buf, nbytes);

			nbytes -= max_bytes;
			}
		if (!alsa_thread_abort && nbytes > 0)
			{
			if (DEBUG_TEST())
				printf("pipe write %d byte\n", nbytes);
			write(alsa_fd_pipe[1], alsa_pipe_buf, nbytes);
			}
		if (   !alsa_thread_abort
			&& (n = snd_pcm_avail_update(pcm)) < alsa_n_frames
		   )
			{
			if (DEBUG_TEST())
	printf("snd_pcm_avail_update=%d, need %d so entering snd_pcm_wait()...\n",
				n, (int) alsa_n_frames);
			snd_pcm_wait(pcm, 1000);
			if (DEBUG_TEST())
				printf("   Returning from snd_pcm_wait()\n");
			}
		}
	if (DEBUG_TEST())
		printf("Aborting ALSA read thread.\n");
	}

static gpointer
gss_alsa_thread(void *data)
	{
	if (DEBUG_TEST())
		printf("gss AlSA thread starting\n");

	alsa_thread_abort = FALSE;
	gss_alsa_sound_card_read();
	gss_alsa_real_close();
	alsa_thread_busy = FALSE;
	alsa_thread_abort = FALSE;
	if (DEBUG_TEST())
		printf("gss AlSA thread done!\n");
	return NULL;
	}

static snd_pcm_t *
gss_alsa_init_fail(snd_pcm_t *pcm, gint err, gchar *what)
	{
	gchar	*msg;

	msg = g_strdup_printf("Can't %s. ALSA device: %s\nsnd error: %s",
				what, alsa_device, snd_strerror(err));

	gkrellm_message_dialog(NULL, msg);
	g_free(msg);
	if (pcm)
		snd_pcm_close(pcm);
	return NULL;
	}

static snd_pcm_t *
gss_alsa_init(void)
	{
	snd_pcm_t			*pcm = NULL;
	snd_pcm_hw_params_t	*params = NULL;
	snd_pcm_uframes_t	buf_size;
	gint				err, dir;
	guint				val;
	guint				rate = SAMPLE_RATE;
	gint				sound_format = SND_PCM_FORMAT_S16_LE;

	snd_pcm_hw_params_alloca(&params);

	if ((err = snd_pcm_open(&pcm, alsa_device,
						SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK)) < 0)
		return gss_alsa_init_fail(pcm, err, "open");

	if ((err = snd_pcm_hw_params_any(pcm, params)) < 0)
		return gss_alsa_init_fail(pcm, err, "set params");

	if ((err = snd_pcm_hw_params_set_access(pcm, params,
						SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
		return gss_alsa_init_fail(pcm, err, "set access");

	if ((err = snd_pcm_hw_params_set_format(pcm, params, sound_format)) < 0)
		return gss_alsa_init_fail(pcm, err, "set format");

	if ((err = snd_pcm_hw_params_set_rate_near(pcm, params, &rate, NULL)) < 0)
		return gss_alsa_init_fail(pcm, err, "set rate");

	if ((err = snd_pcm_hw_params_set_channels(pcm, params, 2)) < 0)
		return gss_alsa_init_fail(pcm, err, "set channels");

	val = ALSA_PERIODS_MIN;
	snd_pcm_hw_params_set_periods_min(pcm, params, &val, &dir);
	if (DEBUG_INIT())
		printf("ALSA set periods min request:  %d  actual: %d  dir: %d\n",
					ALSA_PERIODS_MIN, val, dir);

	err = snd_pcm_hw_params_set_periods_first(pcm, params, &val, &dir);

	/* buf_size has units of frames.
	*/
	buf_size = N_SAMPLES * val;
	if (DEBUG_INIT())
		{
		printf("ALSA set periods first actual: %d  dir: %d\n", val, dir);
		printf("ALSA set buffer size request: %d\n", (int) buf_size);
		}

	if (err < 0)
		return gss_alsa_init_fail(pcm, err, "set periods first");

	/* Try to set the buffer size, but take what is there if we can't.
	*/
	if (snd_pcm_hw_params_set_buffer_size_near(pcm, params, &buf_size) < 0)
		if (snd_pcm_hw_params_set_buffer_size_min(pcm, params, &buf_size) < 0)
			snd_pcm_hw_params_get_buffer_size(params, &buf_size);

	if (DEBUG_INIT())
		printf("ALSA set buffer size actual:  %d\n", (int) buf_size);

	if ((err = snd_pcm_hw_params(pcm, params)) < 0)
		return gss_alsa_init_fail(pcm, err, "set hw params");

	snd_pcm_hw_params_get_period_size(params, &alsa_n_frames, &dir);
	if (DEBUG_INIT())
		printf("ALSA pcm opened OK. n_frames: %d\n", (int) alsa_n_frames);

	return pcm;
	}

static void
gss_alsa_open_stream(void)
	{
	if (   (alsa_thread_busy || gkrellmss->handle)
		&& !gss_alsa_real_close()
	   )
		return;

	gkrellmss->handle = (gpointer) gss_alsa_init();
	if (!gkrellmss->handle)
		{
		gss_alsa_real_close();
		return;
		}
	if (pipe(alsa_fd_pipe) < 0)
		{
		if (DEBUG_TEST())
			perror("Can't create pipe.\n");
		gss_alsa_real_close();
		return;
		}
	gkrellmss->stream_open = TRUE;
	gkrellmss->fd = alsa_fd_pipe[0];
	fcntl(gkrellmss->fd, F_SETFL, O_NONBLOCK);

	if (alsa_pipe_buf == NULL)
		alsa_pipe_buf = g_new0(gchar, alsa_n_frames * sizeof(SoundSample));

	gkrellmss->input_id = gdk_input_add(gkrellmss->fd, GDK_INPUT_READ,
					(GdkInputFunction) gss_alsa_input_read, NULL);

	if (DEBUG_INIT())
		printf("gdk_input_add: %d  pipe size: %lu\n",
				gkrellmss->input_id, alsa_n_frames * sizeof(SoundSample));

	alsa_thread_busy = TRUE;
	g_thread_new("gss_alsa_thread", gss_alsa_thread, NULL);
	}


#define	SDLIB_ALSA_NAME	"ALSA"

static void
gss_alsa_save_config(FILE *f, gchar *keyword)
	{
	fprintf(f, "%s %s name %s\n", keyword, SDLIB_ALSA_NAME, alsa_device);
	}

static void
gss_alsa_load_config(gchar *line)
	{
	if (!strncmp(line, "name", 4))
		{
		g_free(alsa_device);
		alsa_device = g_strdup(line + 5);
		}
	}

static void
gkrellmss_alsa_source_init(void)
	{
	SoundSource	*snd;

	snd = g_new0(SoundSource, 1);
	snd->name = g_strdup(SDLIB_ALSA_NAME);
	snd->type = SOUND_CARD;
	snd->open_stream = gss_alsa_open_stream;
	snd->close_stream = gss_alsa_close_stream;
	snd->load_config = gss_alsa_load_config;
	snd->save_config = gss_alsa_save_config;
	gkrellmss->sound_source_list =
			g_list_append(gkrellmss->sound_source_list, snd);

//	alsa_device = g_strdup("plughw:0,0");
	alsa_device = g_strdup("default");
	}
