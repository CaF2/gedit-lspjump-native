/**
Copyright (c) 2025 Florian Evaldsson

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#include <string.h>
#include <glib/gi18n.h>
#include <gedit/gedit-debug.h>
#include <gedit/gedit-utils.h>
#include <gedit/gedit-app.h>
#include <gedit/gedit-window.h>
#include <gedit/gedit-app-activatable.h>
#include <gedit/gedit-window-activatable.h>
#include "gedit-lspjump.h"

#include "gedit-lspjump-configure-window.h"
#include "gedit-lspjump-configuration.h"
#include "gedit-lspjump-common.h"
#include "gedit-lspjump-rpc.h"

GQueue *GLOBAL_BACK_STACK=NULL;
GQueue *GLOBAL_FORWARD_STACK=NULL;

static void gedit_app_activatable_iface_init(GeditAppActivatableInterface *iface);
static void gedit_window_activatable_iface_init(GeditWindowActivatableInterface *iface);

struct _GeditLspJumpPluginPrivate
{
	GeditWindow *window;
	GSimpleAction *lspjump_definition;
	GSimpleAction *lspjump_reference;
	GSimpleAction *lspjump_undo;
	GSimpleAction *lspjump_redo;
	GSimpleAction *lspjump_settings;
	GeditApp *app;
	GeditMenuExtension *menu_ext;

	//GtkTextIter start, end; /* selection */
};

enum
{
	PROP_0,
	PROP_WINDOW,
	PROP_APP
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED(GeditLspJumpPlugin, gedit_lspjump_plugin, PEAS_TYPE_EXTENSION_BASE, 0,
                               G_IMPLEMENT_INTERFACE_DYNAMIC(GEDIT_TYPE_APP_ACTIVATABLE, gedit_app_activatable_iface_init)
                               G_IMPLEMENT_INTERFACE_DYNAMIC(GEDIT_TYPE_WINDOW_ACTIVATABLE, gedit_window_activatable_iface_init)
                               G_ADD_PRIVATE_DYNAMIC(GeditLspJumpPlugin))

//////////////////////////////////

static void
on_suggestion_clicked(GtkButton *button, gpointer user_data)
{
	g_print("Clicked: %s\n", gtk_button_get_label(button));
}

static gboolean on_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	if (event->keyval == GDK_KEY_Tab && FALSE)
	{
		GtkTextView *text_view=GTK_TEXT_VIEW(widget);
	
		GtkWidget *popover;
		GtkWidget *box;
		GtkWidget *btn1;
		GtkWidget *btn2;

		GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);
		GtkTextIter iter;
		gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer));

		// Get cursor location (in window coordinates)
		GdkRectangle location;
		gtk_text_view_get_iter_location(text_view, &iter, &location);
		gtk_text_view_buffer_to_window_coords(text_view, GTK_TEXT_WINDOW_WIDGET,
			                                  location.x, location.y,
			                                  &location.x, &location.y);

		// Create popover attached to the text_view
		popover = gtk_popover_new(GTK_WIDGET(text_view));
		gtk_widget_set_halign(popover, GTK_ALIGN_START);
		gtk_widget_set_valign(popover, GTK_ALIGN_START);
		gtk_popover_set_pointing_to(GTK_POPOVER(popover), &location);

		// Create content
		box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
		gtk_container_add(GTK_CONTAINER(popover), box);

		btn1 = gtk_button_new_with_label("Suggestion 1");
		btn2 = gtk_button_new_with_label("Suggestion 2");

		g_signal_connect(btn1, "clicked", G_CALLBACK(on_suggestion_clicked), NULL);
		g_signal_connect(btn2, "clicked", G_CALLBACK(on_suggestion_clicked), NULL);

		gtk_box_pack_start(GTK_BOX(box), btn1, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(box), btn2, FALSE, FALSE, 0);

		gtk_widget_show_all(popover);
	}
	return FALSE;
}

static void lspjump_rpc_definition_cb(JsonRpcEndpoint *endpoint, json_t *root, void *user_data)
{
	GeditLspJumpPlugin *plugin=user_data;
	
	json_t *result = json_object_get(root, "result");
	if (!json_is_array(result) || json_array_size(result) == 0)
	{
		fprintf(stderr, "Invalid or empty result array\n");
		json_decref(root);
		return;
	}

	json_t *item = json_array_get(result, 0);
	json_t *uri_obj = json_object_get(item, "uri");
	const char *uri = json_string_value(uri_obj);

	json_t *range = json_object_get(item, "range");
	json_t *start = json_object_get(range, "start");

	int line = json_integer_value(json_object_get(start, "line"));
	int character = json_integer_value(json_object_get(start, "character"));

//	printf("URI: %s\n", uri);
//	printf("Line: %d\n", line);
//	printf("Character: %d\n", character);
	
	g_autoptr(GFile) gfile = g_file_new_for_uri(uri);
	
	GeditWindow *const window=plugin->priv->window;
	
	gedit_lspjump_goto_file_line_column_and_track(window,gfile,line,character);
}

static void lspjump_definition_cb(GAction *action, GVariant *parameter, GeditLspJumpPlugin *plugin)
{
	GFile *gfile=lspjump_get_active_file_from_window(plugin->priv->window);
	
	g_autofree gchar *file_path = g_file_get_path(gfile);
	
	g_autofree gchar *text=get_full_text_from_active_document(plugin->priv->window);
	
	GeditTab *tab = gedit_window_get_active_tab(plugin->priv->window);
	if (!tab)
	{
		g_print("No active tab.\n");
		return;
	}

	GeditDocument *doc = gedit_tab_get_document(tab);

	GtkTextIter iter;
	GtkTextMark *mark = gtk_text_buffer_get_insert(GTK_TEXT_BUFFER(doc));
	gtk_text_buffer_get_iter_at_mark(GTK_TEXT_BUFFER(doc), &iter, mark);

	gint line = gtk_text_iter_get_line(&iter); // Zero-based line number
	gint offset = gtk_text_iter_get_offset(&iter); // Offset from start of buffer
	gint line_offset = gtk_text_iter_get_line_offset(&iter); // Offset within the line
	
	lspjump_rpc_definition(file_path,text,line,line_offset,lspjump_rpc_definition_cb,plugin);
}

static void on_item_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget *window = user_data;
	GeditLspJumpPlugin *plugin=g_object_get_data(G_OBJECT(window), "plugin");
	
	const char *uri=g_object_get_data(G_OBJECT(button), "uri");
	intptr_t line=(intptr_t)g_object_get_data(G_OBJECT(button), "line");
	intptr_t character=(intptr_t)g_object_get_data(G_OBJECT(button), "character");

	g_autoptr(GFile) gfile = g_file_new_for_uri(uri);

	const gchar *label = gtk_button_get_label(button);
	g_print("Clicked on: %s\n", label);
	
	gedit_lspjump_goto_file_line_column_and_track(plugin->priv->window,gfile,line,character);
	// You would jump to the file/line here
	gtk_widget_destroy(window);
}

static void lspjump_rpc_reference_cb(JsonRpcEndpoint *endpoint, json_t *root, void *user_data)
{
	GeditLspJumpPlugin *plugin=user_data;
	
	json_t *results = json_object_get(root, "result");
	if (!json_is_array(results))
	{
		return;
	}
	
	if(json_array_size(results)>0)
	{
		// Create a popup window
		GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(window), "Locations");
		gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(plugin->priv->window))));
		gtk_window_set_modal(GTK_WINDOW(window), TRUE);
		gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
		g_object_set_data(G_OBJECT(window), "plugin", plugin);

		GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
		gtk_container_add(GTK_CONTAINER(window), scrolled);

		GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
		gtk_container_add(GTK_CONTAINER(scrolled), vbox);
		gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

		// Iterate over results
		size_t index;
		json_t *item;
		json_array_foreach(results, index, item)
		{
			json_t *uri = json_object_get(item, "uri");
			json_t *range = json_object_get(item, "range");
			json_t *start = json_object_get(range, "start");
			json_t *line = json_object_get(start, "line");
			json_t *character = json_object_get(start, "character");

			const char *uri_str = json_string_value(uri);
			json_int_t line_num = json_integer_value(line);
			json_int_t character_num = json_integer_value(character);

			// Shorten file path to basename
			g_autofree gchar *file_basename = g_path_get_basename(uri_str);
			g_autofree gchar *label_text = g_strdup_printf("%s:%d", file_basename, line_num + 1);

			GtkWidget *button = gtk_button_new_with_label(label_text);
			g_signal_connect(button, "clicked", G_CALLBACK(on_item_clicked), window);
			g_object_set_data_full(G_OBJECT(button), "uri", g_strdup(uri_str),g_free);
			g_object_set_data(G_OBJECT(button), "line", (void*)(intptr_t)line_num);
			g_object_set_data(G_OBJECT(button), "character", (void*)(intptr_t)character_num);
			gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
		}

		gtk_widget_show_all(window);
	}
}

static void lspjump_reference_cb(GAction *action, GVariant *parameter, GeditLspJumpPlugin *plugin)
{
	GFile *gfile=lspjump_get_active_file_from_window(plugin->priv->window);
	
	g_autofree gchar *file_path = g_file_get_path(gfile);
	
	g_autofree gchar *text=get_full_text_from_active_document(plugin->priv->window);
	
	GeditTab *tab = gedit_window_get_active_tab(plugin->priv->window);
	if (!tab)
	{
		g_print("No active tab.\n");
		return;
	}

	GeditDocument *doc = gedit_tab_get_document(tab);

	GtkTextIter iter;
	GtkTextMark *mark = gtk_text_buffer_get_insert(GTK_TEXT_BUFFER(doc));
	gtk_text_buffer_get_iter_at_mark(GTK_TEXT_BUFFER(doc), &iter, mark);

	gint line = gtk_text_iter_get_line(&iter); // Zero-based line number
	gint offset = gtk_text_iter_get_offset(&iter); // Offset from start of buffer
	gint line_offset = gtk_text_iter_get_line_offset(&iter); // Offset within the line
	
	lspjump_rpc_reference(file_path,text,line,line_offset,lspjump_rpc_reference_cb,plugin);
}

static void lspjump_undo_cb(GAction *action, GVariant *parameter, GeditLspJumpPlugin *plugin)
{
	gedit_lspjump_do_undo(plugin->priv->window);
}

static void lspjump_redo_cb(GAction *action, GVariant *parameter, GeditLspJumpPlugin *plugin)
{
	gedit_lspjump_do_redo(plugin->priv->window);
}

static void lspjump_settings_cb(GAction *action, GVariant *parameter, GeditLspJumpPlugin *plugin)
{
	create_settings_window(GTK_WIDGET(plugin->priv->app),plugin->priv->window);
}

static void update_ui(GeditLspJumpPlugin *plugin)
{
	GeditView *view;

	view = gedit_window_get_active_view(plugin->priv->window);

	g_simple_action_set_enabled(plugin->priv->lspjump_definition, (view != NULL) && gtk_text_view_get_editable(GTK_TEXT_VIEW(view)));
	g_simple_action_set_enabled(plugin->priv->lspjump_reference, (view != NULL) && gtk_text_view_get_editable(GTK_TEXT_VIEW(view)));
	g_simple_action_set_enabled(plugin->priv->lspjump_undo, (view != NULL) && gtk_text_view_get_editable(GTK_TEXT_VIEW(view)));
	g_simple_action_set_enabled(plugin->priv->lspjump_redo, (view != NULL) && gtk_text_view_get_editable(GTK_TEXT_VIEW(view)));
	g_simple_action_set_enabled(plugin->priv->lspjump_settings, (view != NULL) && gtk_text_view_get_editable(GTK_TEXT_VIEW(view)));
}

static void gedit_lspjump_plugin_app_activate(GeditAppActivatable *activatable)
{
	GeditLspJumpPluginPrivate *priv;
	GMenuItem *item;

	priv = GEDIT_LSPJUMP_PLUGIN(activatable)->priv;
	
	gtk_application_set_accels_for_action(GTK_APPLICATION(priv->app), "win.definition", (const gchar *[]){"F3", NULL});
	gtk_application_set_accels_for_action(GTK_APPLICATION(priv->app), "win.reference", (const gchar *[]){"F4", NULL});
	gtk_application_set_accels_for_action(GTK_APPLICATION(priv->app), "win.lspjump_undo", (const gchar *[]){"<Alt>B", NULL});
	gtk_application_set_accels_for_action(GTK_APPLICATION(priv->app), "win.lspjump_redo", (const gchar *[]){"<Alt><Shift>B", NULL});
	gtk_application_set_accels_for_action(GTK_APPLICATION(priv->app), "win.lspjump_settings", (const gchar *[]){"F5", NULL});
//	gtk_application_set_accels_for_action(GTK_APPLICATION(priv->app), "win.uncomment", (const gchar *[]){"<Primary><Shift>M", NULL});
	
	priv->menu_ext = gedit_app_activatable_extend_menu(activatable, "tools-section");

	item = g_menu_item_new(_("Goto definition"), "win.definition");
	gedit_menu_extension_append_menu_item(priv->menu_ext, item);
	g_object_unref(item);
	item = g_menu_item_new(_("Goto reference"), "win.reference");
	gedit_menu_extension_append_menu_item(priv->menu_ext, item);
	g_object_unref(item);
	item = g_menu_item_new(_("Undo"), "win.lspjump_redo");
	gedit_menu_extension_append_menu_item(priv->menu_ext, item);
	g_object_unref(item);
	item = g_menu_item_new(_("Redo"), "win.lspjump_undo");
	gedit_menu_extension_append_menu_item(priv->menu_ext, item);
	g_object_unref(item);
	item = g_menu_item_new(_("Settings"), "win.lspjump_settings");
	gedit_menu_extension_append_menu_item(priv->menu_ext, item);
	g_object_unref(item);
}

static void gedit_lspjump_plugin_app_deactivate(GeditAppActivatable *activatable)
{
	GeditLspJumpPluginPrivate *priv;

	priv = GEDIT_LSPJUMP_PLUGIN(activatable)->priv;

	g_clear_object(&priv->menu_ext);
}

static void on_tab_changed(GeditWindow *window, gpointer user_data)
{
	GeditView *view = gedit_window_get_active_view(window);
	if (view)
	{
		void *is_loaded=g_object_get_data(G_OBJECT(view), "ll");
		if(is_loaded==NULL)
		{
			g_object_set_data(G_OBJECT(view), "ll", "y");
			// Ensure each new tab gets the key-press-event handler
			g_signal_connect(view, "key-press-event", G_CALLBACK(on_key_press_event), user_data);
		}
	}
}

static void gedit_lspjump_plugin_window_activate(GeditWindowActivatable *activatable)
{
	GeditLspJumpPlugin *plugin = GEDIT_LSPJUMP_PLUGIN(activatable);

	GeditLspJumpPluginPrivate *priv;

	priv = GEDIT_LSPJUMP_PLUGIN(activatable)->priv;

	priv->lspjump_definition = g_simple_action_new("definition", NULL);
	g_signal_connect(priv->lspjump_definition, "activate", G_CALLBACK(lspjump_definition_cb), activatable);
	g_action_map_add_action(G_ACTION_MAP(priv->window), G_ACTION(priv->lspjump_definition));
	
	priv->lspjump_reference = g_simple_action_new("reference", NULL);
	g_signal_connect(priv->lspjump_reference, "activate", G_CALLBACK(lspjump_reference_cb), activatable);
	g_action_map_add_action(G_ACTION_MAP(priv->window), G_ACTION(priv->lspjump_reference));
	
	priv->lspjump_undo = g_simple_action_new("lspjump_undo", NULL);
	g_signal_connect(priv->lspjump_undo, "activate", G_CALLBACK(lspjump_undo_cb), activatable);
	g_action_map_add_action(G_ACTION_MAP(priv->window), G_ACTION(priv->lspjump_undo));
	
	priv->lspjump_redo = g_simple_action_new("lspjump_redo", NULL);
	g_signal_connect(priv->lspjump_redo, "activate", G_CALLBACK(lspjump_redo_cb), activatable);
	g_action_map_add_action(G_ACTION_MAP(priv->window), G_ACTION(priv->lspjump_redo));
	
	priv->lspjump_settings = g_simple_action_new("lspjump_settings", NULL);
	g_signal_connect(priv->lspjump_settings, "activate", G_CALLBACK(lspjump_settings_cb), activatable);
	g_action_map_add_action(G_ACTION_MAP(priv->window), G_ACTION(priv->lspjump_settings));
	
	update_ui(GEDIT_LSPJUMP_PLUGIN(activatable));
	
	g_signal_connect(priv->window, "active-tab-changed", G_CALLBACK(on_tab_changed), plugin);
}

static void gedit_lspjump_plugin_window_deactivate(GeditWindowActivatable *activatable)
{
	GeditLspJumpPluginPrivate *priv;

	priv = GEDIT_LSPJUMP_PLUGIN(activatable)->priv;
	g_action_map_remove_action(G_ACTION_MAP(priv->window), "lspjump");
}

static void gedit_lspjump_plugin_window_update_state(GeditWindowActivatable *activatable)
{
	update_ui(GEDIT_LSPJUMP_PLUGIN(activatable));
}

static void gedit_lspjump_plugin_init(GeditLspJumpPlugin *plugin)
{
	plugin->priv = gedit_lspjump_plugin_get_instance_private(plugin);
}

static void gedit_lspjump_plugin_dispose(GObject *object)
{
	GeditLspJumpPlugin *plugin = GEDIT_LSPJUMP_PLUGIN(object);

	g_clear_object(&plugin->priv->lspjump_definition);
	g_clear_object(&plugin->priv->lspjump_reference);
	g_clear_object(&plugin->priv->lspjump_undo);
	g_clear_object(&plugin->priv->lspjump_redo);
	g_clear_object(&plugin->priv->lspjump_settings);
	
	g_clear_object(&plugin->priv->window);
	g_clear_object(&plugin->priv->menu_ext);
	g_clear_object(&plugin->priv->app);

	G_OBJECT_CLASS(gedit_lspjump_plugin_parent_class)->dispose(object);
}

static void gedit_lspjump_plugin_finalize(GObject *object)
{
	G_OBJECT_CLASS(gedit_lspjump_plugin_parent_class)->finalize(object);
}

static void gedit_lspjump_plugin_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GeditLspJumpPlugin *plugin = GEDIT_LSPJUMP_PLUGIN(object);

	switch (prop_id)
	{
	case PROP_WINDOW:
		plugin->priv->window = GEDIT_WINDOW(g_value_dup_object(value));
		break;
	case PROP_APP:
		plugin->priv->app = GEDIT_APP(g_value_dup_object(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void gedit_lspjump_plugin_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GeditLspJumpPlugin *plugin = GEDIT_LSPJUMP_PLUGIN(object);

	switch (prop_id)
	{
	case PROP_WINDOW:
		g_value_set_object(value, plugin->priv->window);
		break;
	case PROP_APP:
		g_value_set_object(value, plugin->priv->app);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void gedit_lspjump_plugin_class_init(GeditLspJumpPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->dispose = gedit_lspjump_plugin_dispose;
	object_class->finalize = gedit_lspjump_plugin_finalize;
	object_class->set_property = gedit_lspjump_plugin_set_property;
	object_class->get_property = gedit_lspjump_plugin_get_property;
	
	GLOBAL_BACK_STACK=g_queue_new();
	GLOBAL_FORWARD_STACK=g_queue_new();
	
	load_configuration();

	g_object_class_override_property(object_class, PROP_WINDOW, "window");
	g_object_class_override_property(object_class, PROP_APP, "app");
}

static void gedit_lspjump_plugin_class_finalize(GeditLspJumpPluginClass *klass)
{
}

static void gedit_app_activatable_iface_init(GeditAppActivatableInterface *iface)
{
	iface->activate = gedit_lspjump_plugin_app_activate;
	iface->deactivate = gedit_lspjump_plugin_app_deactivate;
}

static void gedit_window_activatable_iface_init(GeditWindowActivatableInterface *iface)
{
	iface->activate = gedit_lspjump_plugin_window_activate;
	iface->deactivate = gedit_lspjump_plugin_window_deactivate;
	iface->update_state = gedit_lspjump_plugin_window_update_state;
}

G_MODULE_EXPORT void peas_register_types(PeasObjectModule *module)
{
	gedit_lspjump_plugin_register_type(G_TYPE_MODULE(module));

	peas_object_module_register_extension_type(module, GEDIT_TYPE_APP_ACTIVATABLE, GEDIT_TYPE_LSPJUMP_PLUGIN);
	peas_object_module_register_extension_type(module, GEDIT_TYPE_WINDOW_ACTIVATABLE, GEDIT_TYPE_LSPJUMP_PLUGIN);
}
