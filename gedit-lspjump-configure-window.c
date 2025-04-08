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
#include "gedit-lspjump-configure-window.h"

#include "gedit-lspjump-rpc.h"
#include "gedit-lspjump-common.h"

// Placeholder for the settings structure or methods
typedef struct
{
	const char *PROJECT_PATH;
	const char *LSP_BIN;
	const char *LSP_BIN_ARGS;
	const char *LSP_SEARCH_PATH;
	const char *LSP_SETTINGS;
	void *SETTINGS_DATA;
	void *LSP_NAVIGATOR;
	void *SETTINGS_LANGUAGE;
	int (*debugprint)(const char *);
} settings_t;

settings_t GLOBAL_SETTINGS_OBJ = {.PROJECT_PATH = "/path/to/project",
                       .LSP_BIN = "/path/to/lsp/bin",
                       .LSP_BIN_ARGS = "",
                       .LSP_SEARCH_PATH = "search_path",
                       .LSP_SETTINGS = "settings",
                       .SETTINGS_DATA = NULL,
                       .LSP_NAVIGATOR = NULL,
                       .SETTINGS_LANGUAGE = NULL,
                       .debugprint = puts};

// Placeholder function for LanguageSettings dialog
typedef struct
{
	GtkWidget *lang_name;
	GtkWidget *name;
	GtkWidget *bin_entry;
	GtkWidget *bin_args_entry;
	GtkWidget *search_entry;
	GtkTextBuffer *tbuffer;
} LanguageSettings;

static void lang_settings_debug(LanguageSettings *dialog)
{
	GLOBAL_SETTINGS_OBJ.debugprint("empty");
}

static void set_lsp_config(LanguageSettings *dialog, const char *name, const char *lang_name, const char *bin_entry,
                           const char *bin_args_entry, const char *search_entry, const char *settings, gboolean flag)
{
	GLOBAL_SETTINGS_OBJ.debugprint("empty");
}

// Callbacks
static void _change_project_path(GtkWidget *widget, GtkWidget *path_entry)
{
	const char *new_path = gtk_entry_get_text(GTK_ENTRY(path_entry));
	GLOBAL_SETTINGS_OBJ.debugprint("Changed to: ");
	GLOBAL_SETTINGS_OBJ.debugprint(new_path);
	
	lspjump_rpc_init(new_path);
	// Placeholder for project path change functionality
}

static void _new_language(GtkWidget *widget, GtkWidget *lang_cb)
{
	// Placeholder for creating new language
	GLOBAL_SETTINGS_OBJ.debugprint("New language creation: empty");
}

static void _remove_language(GtkWidget *widget, GtkWidget *lang_cb)
{
	const char *profile_name = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(lang_cb));
	GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(widget), 0, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
	                                           "Do you want to remove the language \"%s\"?", profile_name);
	int response = gtk_dialog_run(GTK_DIALOG(dialog));
	if (response == GTK_RESPONSE_YES)
	{
		GLOBAL_SETTINGS_OBJ.debugprint("Language removed: ");
		GLOBAL_SETTINGS_OBJ.debugprint(profile_name);
		// Placeholder for language removal
	}
	gtk_widget_destroy(dialog);
}

static void _edit_language(GtkWidget *widget, GtkWidget *lang_cb)
{
	const char *profile_name = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(lang_cb));
	GLOBAL_SETTINGS_OBJ.debugprint("Editing language: ");
	GLOBAL_SETTINGS_OBJ.debugprint(profile_name);
	// Placeholder for editing language
}

static void _get_proj(GtkWidget *widget, GtkWidget *path_entry)
{
	GLOBAL_SETTINGS_OBJ.debugprint("Get proj");
	
	GeditWindow *window=g_object_get_data(G_OBJECT(path_entry), "window-obj");

	GFile *gfile=lspjump_get_active_file_from_window(window);
	
	g_autoptr(GFile) parent_path=g_file_get_parent(gfile);
	
	g_autofree gchar *folder_path = g_file_get_path(parent_path);
	
	// Placeholder for getting project directory
	gtk_entry_set_text(GTK_ENTRY(path_entry), folder_path);
}

static char *find_parent_file_path(GFile *gfile, const gchar *target_filename)
{
	g_autoptr(GFile) current = g_file_dup(gfile); // Start from current file
	while (current != NULL)
	{
		// Build a GFile pointing to the target inside this folder
		g_autoptr(GFile) candidate = g_file_get_child(current, target_filename);
		if (g_file_query_exists(candidate, NULL))
		{
			// Found the file!
			gchar *found_path = g_file_get_path(current);
			g_print("Found in: %s\n", found_path);
			return found_path;
		}

		// Move one level up
		g_autoptr(GFile) parent = g_file_get_parent(current);
		if (parent == NULL)
		{
			g_print("Reached filesystem root, file not found.\n");
			break;
		}
		current = g_steal_pointer(&parent); // Transfer ownership
	}
	
	return NULL;
}

static void _search_proj(GtkWidget *widget, GtkWidget *path_entry)
{
	const gchar *target_filename = "compile_commands.json";

	GLOBAL_SETTINGS_OBJ.debugprint("Get proj");
	
	GeditWindow *window=g_object_get_data(G_OBJECT(path_entry), "window-obj");

	GFile *gfile=lspjump_get_active_file_from_window(window);
	
	g_autoptr(GFile) parent_path=g_file_get_parent(gfile);
	
	g_autofree gchar *folder_path = find_parent_file_path(parent_path,target_filename);
	
	// Placeholder for getting project directory
	gtk_entry_set_text(GTK_ENTRY(path_entry), folder_path);
}

static void _generate_language_combo(GtkWidget *lang_cb)
{
	GLOBAL_SETTINGS_OBJ.debugprint("empty");
	// Placeholder for generating language combo
}

static void _generate_path_history(GtkWidget *path_history_cb)
{
	GLOBAL_SETTINGS_OBJ.debugprint("empty");
	// Placeholder for generating path history
}

static void _click_histoy_path(GtkComboBox *combo_box, gpointer user_data)
{
	const char *new_path = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo_box));
	GLOBAL_SETTINGS_OBJ.debugprint("empty");
	// Placeholder for path history click functionality
}

static void on_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data)
{
    if (response_id == GTK_RESPONSE_CLOSE)
    {
        gtk_widget_destroy(GTK_WIDGET(dialog));  // Close and destroy the dialog
    }
}

GtkWidget *create_settings_window(GtkWidget *parent, GeditWindow *window)
{
	GtkWidget *grid = gtk_grid_new();
	GtkWidget *label, *button, *path_entry, *path_history_cb, *lang_cb;
	
	GtkWidget *dialog = gtk_dialog_new_with_buttons(
		"Settings",
		GTK_WINDOW(gtk_widget_get_toplevel(parent)),
		GTK_DIALOG_MODAL,
		"Close", GTK_RESPONSE_CLOSE,
		NULL);
	
	g_signal_connect(dialog, "response", G_CALLBACK(on_dialog_response), NULL);
	
	GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_add(GTK_CONTAINER(content_area), grid);
	gtk_widget_set_halign(grid, GTK_ALIGN_FILL);
	gtk_widget_set_valign(grid, GTK_ALIGN_FILL);
	gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
	gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
	gtk_widget_set_hexpand(grid, TRUE);
	gtk_widget_set_vexpand(grid, TRUE);
	
	// Label and buttons setup
	label = gtk_label_new("Project path:");
	gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 2, 1);

	path_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(path_entry), GLOBAL_SETTINGS_OBJ.PROJECT_PATH);
	g_object_set_data(G_OBJECT(path_entry), "window-obj", window);
	gtk_grid_attach(GTK_GRID(grid), path_entry, 0, 1, 1, 1);

	button = gtk_button_new_with_label("Change");
	g_signal_connect(button, "clicked", G_CALLBACK(_change_project_path), path_entry);
	gtk_grid_attach(GTK_GRID(grid), button, 1, 1, 1, 1);

	button = gtk_button_new_with_label("Get project dir");
	g_signal_connect(button, "clicked", G_CALLBACK(_get_proj), path_entry);
	gtk_grid_attach(GTK_GRID(grid), button, 0, 2, 1, 1);

	button = gtk_button_new_with_label("Search project dir");
	g_signal_connect(button, "clicked", G_CALLBACK(_search_proj), path_entry);
	gtk_grid_attach(GTK_GRID(grid), button, 1, 2, 1, 1);

	// Profiles setup
	label = gtk_label_new("Profiles");
	gtk_grid_attach(GTK_GRID(grid), label, 0, 3, 2, 1);

	// Language ComboBox setup
	lang_cb = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(lang_cb), "Profile 1");
	gtk_grid_attach(GTK_GRID(grid), lang_cb, 0, 4, 2, 1);

	// Buttons for profile actions
	button = gtk_button_new_with_label("Edit");
	g_signal_connect(button, "clicked", G_CALLBACK(_edit_language), lang_cb);
	gtk_grid_attach(GTK_GRID(grid), button, 0, 5, 2, 1);

	button = gtk_button_new_with_label("New");
	g_signal_connect(button, "clicked", G_CALLBACK(_new_language), lang_cb);
	gtk_grid_attach(GTK_GRID(grid), button, 0, 6, 1, 1);

	button = gtk_button_new_with_label("Remove");
	g_signal_connect(button, "clicked", G_CALLBACK(_remove_language), lang_cb);
	gtk_grid_attach(GTK_GRID(grid), button, 1, 6, 1, 1);

	// Generate path history and language combo
	_generate_path_history(path_history_cb);
	_generate_language_combo(lang_cb);

	gtk_widget_show_all(dialog);
	return dialog;
}
