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

#include <esd.h>

/* The format of the esd data to monitor.  STEREO is interleaved channels */
#define SOUND_FORMAT (ESD_BITS16 | ESD_STEREO | ESD_STREAM | ESD_PLAY)


static void
gss_esd_close_stream(void)
	{
	if (gkrellmss->fd >= 0)
		esd_close(gkrellmss->fd);
	reset_sound_data();
	}

static void
gss_esd_input_read(gpointer data, gint source, GdkInputCondition condition)
	{
	gint	count;

	count = read(source, gkrellmss->buffer,
					gkrellmss->buf_len * sizeof(SoundSample));
	if (count > 0)
		process_sound_samples(count);
	else
		gss_esd_close_stream();
	}

static void
gss_esd_open_stream(void)
	{
	gkrellmss->fd = esd_monitor_stream(SOUND_FORMAT, SAMPLE_RATE, NULL,
						"gkrellmss");
	if (gkrellmss->fd < 0)
		{
		gkrellmss->stream_open = FALSE;
		gss_esd_close_stream();
		gkrellm_message_dialog(NULL, "Can't connect to esd server.");
		return;
		}
	gkrellmss->stream_open = TRUE;
	gkrellmss->input_id = gdk_input_add(gkrellmss->fd, GDK_INPUT_READ,
						(GdkInputFunction) gss_esd_input_read, NULL);
	}

static void
cb_gss_esd_control(gpointer data, guint action, GtkWidget *w)
	{
	gchar	*argv[3];
	gint	n, fd = -1;
	gchar	buf[128];
	GError	*err = NULL;
	gboolean	res;

	if (action == 0)
		res = g_spawn_command_line_async("esdctl standby", &err);
	else if (action == 1)
		res = g_spawn_command_line_async("esdctl resume", &err);
	else if (action == 2)
		{
		argv[0] = "esdctl";
		argv[1] = "standbymode";
		argv[2] = NULL;

		res = g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
				NULL, NULL, NULL, NULL, &fd, NULL, &err);
		if (fd >= 0)
			{
			n = read(fd, buf, sizeof(buf) - 1);
			if (n > 0)
				{
				if (buf[n - 1] == '\n')
					--n;
				buf[n] = '\0';
				gkrellm_message_dialog(NULL, buf);
				}
			close(fd);
			}
		}
	else
		return;

	if (!res && err)
		{
		gkrellm_message_dialog(NULL, err->message);
		g_error_free(err);
		}
	}

static GtkItemFactoryEntry esound_factory[] =
	{
	{ N_("/Esound Control"),		NULL,	NULL,				0, "<Branch>"},
	{ N_("/Esound Control/Standby"), NULL,	cb_gss_esd_control,	0,	"<Item>"},
	{ N_("/Esound Control/Resume"),	NULL,	cb_gss_esd_control,	1,	"<Item>"},
	{ N_("/Esound Control/Status"),	NULL,	cb_gss_esd_control,	2,	"<Item>"},
	};

static void
gss_esd_option_menu(GtkItemFactory *factory)
	{
	gint	i, n;

	n = sizeof(esound_factory) / sizeof(GtkItemFactoryEntry);
	for (i = 0; i < n; ++i)
		esound_factory[i].path = _(esound_factory[i].path);
	gtk_item_factory_create_items(factory, n, esound_factory, NULL);
	}

static void
gkrellmss_esd_source_init(void)
	{
	SoundSource	*snd;

	snd = g_new0(SoundSource, 1);
	snd->name = g_strdup("Esound");
	snd->type = SOUND_SERVER;
	snd->open_stream = gss_esd_open_stream;
	snd->close_stream = gss_esd_close_stream;
	snd->option_menu = gss_esd_option_menu;

	gkrellmss->sound_source_list =
			g_list_append(gkrellmss->sound_source_list, snd);
	}


