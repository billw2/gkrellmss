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

static GtkItemFactory	*option_factory;

static gint				disable_select;


  /* gtk_item_factory_create_items() uses callback_type 1
  */
static void
cb_gkrellmss_option_menu(gpointer data, guint action, GtkWidget *w)
	{
	GList		*list;
	SoundSource	*snd_src;
	gint		active;

	if (disable_select)
		return;
	list = g_list_nth(gkrellmss->sound_source_list, action);
	snd_src = (SoundSource *) list->data;
	gkrellmss->sound_source_index = action;

	active = GTK_CHECK_MENU_ITEM(w)->active;

	if (!active && gkrellmss->sound_source == snd_src)
		{
		if (DEBUG())
			printf("closing %s\n", snd_src->name);
		(*snd_src->close_stream)();
		}
	if (active)
		{
		if (gkrellmss->stream_open)
			{
			if (DEBUG())
				printf("closing %s\n", gkrellmss->sound_source->name);
			(*gkrellmss->sound_source->close_stream)();
			}
		if (DEBUG())
			printf("opening %s\n", snd_src->name);
		(*snd_src->open_stream)();
		gkrellmss->sound_source = snd_src;
		}
	gkrellm_config_modified();
	gkrellmss_sound_chart_draw(FORCE_RESET, DRAW_GRID);
	}


static GtkItemFactoryEntry	separator_entry =
	{"/-", NULL, 0, 0, "<Separator>"};

static GtkItemFactoryEntry	factory_entry =
	{N_("/Sound Source"),	NULL, 0, 0, "<Branch>"};

void
gkrellmss_option_menu_build(void)
	{
	GtkAccelGroup	*accel;
	GList			*list;
	SoundSource		*snd;
	gchar			*s, *path, *radio_path = NULL;
	gint			i;

	accel = gtk_accel_group_new();
	option_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<Main>", accel);
	gtk_window_add_accel_group(GTK_WINDOW(gkrellm_get_top_window()), accel);

	gtk_item_factory_create_item(option_factory, &separator_entry, NULL, 1);

	path = _(factory_entry.path);
	factory_entry.path = path;
	gtk_item_factory_create_item(option_factory, &factory_entry, NULL, 1);

	factory_entry.callback = cb_gkrellmss_option_menu;

	for (i = 0, list = gkrellmss->sound_source_list; list; list = list->next)
		{
		snd = (SoundSource *) list->data;
		s = g_strdup_printf("%s/%s", path, snd->name);
		factory_entry.path = s;
		snd->preselect_item = s;
		if (!radio_path)
			{
			factory_entry.item_type = "<RadioItem>";
			radio_path = g_strdup(s);
			}
		else
			factory_entry.item_type = radio_path;
		factory_entry.callback_action = i++;
		gtk_item_factory_create_item(option_factory, &factory_entry, NULL, 1);
		}
	g_free(radio_path);

	gtk_item_factory_create_item(option_factory, &separator_entry, NULL, 1);

	for (list = gkrellmss->sound_source_list; list; list = list->next)
		{
		snd = (SoundSource *) list->data;
		if (snd->option_menu)
			(*snd->option_menu)(option_factory);
		}
	}


void
gkrellmss_option_menu(GdkEventButton *ev)
	{
	disable_select = TRUE;
	gtk_check_menu_item_set_active(
			GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(option_factory,
			gkrellmss->sound_source->preselect_item)), TRUE);
	disable_select = FALSE;

	gtk_menu_popup(GTK_MENU(option_factory->widget), NULL, NULL, NULL, NULL,
				ev->button, ev->time);
	}

