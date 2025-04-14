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
#include "gedit-lspjump-configuration.h"

enum
{
	COLUMN_TEXT,
	COLUMN_OBJECT,
	NUM_COLUMNS
};

static void _update_language_combo_box(GeditWindow *window, GtkWidget *lang_cb, GtkTreeModel *model)
{
	GtkListStore *store=GTK_LIST_STORE(model);

	gtk_list_store_clear(store);

	const char *current_language=get_programming_language(window);
	int default_item=0;
	int cur_add_item=0;
	
	for(int i=0;i<GLOBAL_LSPJUMP_CONFIGURATIONS->len;i++)
	{
		LspJumpConfigurationFile *conf=g_ptr_array_index(GLOBAL_LSPJUMP_CONFIGURATIONS,i);
		
		xmlNode *root = xmlDocGetRootElement(conf->doc);

		for (xmlNode *node = root->children; node; node = node->next)
		{
			if (node->type == XML_ELEMENT_NODE && xmlStrcmp(node->name, (const xmlChar *)"language") == 0)
			{
				g_autofree xmlChar *name = xmlGetProp(node, (const xmlChar *)"name");
				
				xmlNode *lsp_language_node=xml_get_child_by_tag(node,"lsp_language");
				xmlChar *lsp_language=xmlNodeGetContent(lsp_language_node);
				xmlNode *lsp_bin_node=xml_get_child_by_tag(node,"lsp_bin");
				xmlChar *lsp_bin=xmlNodeGetContent(lsp_bin_node);
				xmlNode *lsp_bin_args_node=xml_get_child_by_tag(node,"lsp_bin_args");
				xmlChar *lsp_bin_args=xmlNodeGetContent(lsp_bin_args_node);
				xmlNode *lsp_search_node=xml_get_child_by_tag(node,"lsp_search");
				xmlChar *lsp_search=xmlNodeGetContent(lsp_search_node);
				xmlNode *lsp_settings_node=xml_get_child_by_tag(node,"lsp_settings");
				xmlChar *lsp_settings=xmlNodeGetContent(lsp_settings_node);
				
				GtkTreeIter iter;
				gtk_list_store_append(store, &iter);
				GObject *obj1 = g_object_new(G_TYPE_OBJECT, NULL);
				g_object_set_data_full(obj1, "lsp_language", lsp_language, g_free);
				g_object_set_data_full(obj1, "lsp_bin", lsp_bin, g_free);
				g_object_set_data_full(obj1, "lsp_bin_args", lsp_bin_args, g_free);
				g_object_set_data_full(obj1, "lsp_search", lsp_search, g_free);
				g_object_set_data_full(obj1, "lsp_settings", lsp_settings, g_free);
				g_object_set_data_full(obj1, "xml_node", node, NULL);
				g_object_set_data_full(obj1, "xml_file", conf, NULL);
				
				gtk_list_store_set(store, &iter, COLUMN_TEXT, name, COLUMN_OBJECT, obj1, -1);
				
				g_auto(GStrv) result = g_strsplit(lsp_language, ",", -1);
				
				for (int j = 0; result[j] != NULL; j++)
				{
					if(g_ascii_strcasecmp(current_language,result[j])==0)
					{
						fprintf(stdout,"%s:%d Found default: [%s %s]\n",__FILE__,__LINE__,name,lsp_language);
						default_item=cur_add_item;
					}
				}
				
				cur_add_item++;
//				gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(lang_cb), name);
			}
		}
	}
	
	gtk_combo_box_set_active(GTK_COMBO_BOX(lang_cb), default_item);
}

// Callbacks
static void _change_project_path(GtkWidget *widget, GtkWidget *path_entry)
{
	GtkWidget *curr_lang_combo=g_object_get_data(G_OBJECT(widget), "curr_lang");
	GtkTreeModel *curr_lang_model=g_object_get_data(G_OBJECT(widget), "curr_lang_model");
	
	const char *new_path = gtk_entry_get_text(GTK_ENTRY(path_entry));
	puts("Changed to: ");
	puts(new_path);
	
	GtkTreeIter iter;
	
	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(curr_lang_combo), &iter))
	{
		GObject *obj = NULL;
		gtk_tree_model_get(curr_lang_model, &iter, COLUMN_OBJECT, &obj, -1);

		if (obj)
		{
			const char *lsp_language=g_object_get_data(obj, "lsp_language");
			const char *lsp_bin=g_object_get_data(obj, "lsp_bin");
			const char *lsp_bin_args=g_object_get_data(obj, "lsp_bin_args");
			const char *lsp_search=g_object_get_data(obj, "lsp_search");
			const char *lsp_settings=g_object_get_data(obj, "lsp_settings");
			
			lspjump_rpc_init(new_path,lsp_bin,lsp_bin_args,lsp_settings);
			
			// Unref when you're done if needed
//			g_object_unref(obj);

			return;
		}
	}
	
	
	// Placeholder for project path change functionality
}

void xml_set_or_replace_child_content(xmlNodePtr parent, const char *child_name, const char *content)
{
	xmlNodePtr cur = NULL;

	// Search for an existing child with the given name
	for (cur = parent->children; cur != NULL; cur = cur->next)
	{
		if (cur->type == XML_ELEMENT_NODE && xmlStrcmp(cur->name, (const xmlChar *)child_name) == 0)
		{
			// Found existing child, replace its content
			xmlNodeSetContent(cur, (const xmlChar *)content);
			return;
		}
	}

	// If not found, create new child
	xmlNewChild(parent, NULL, (const xmlChar *)child_name, (const xmlChar *)content);
}

static void on_create_language_settings_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data)
{
	GeditWindow *window=user_data;
	GtkWidget *curr_lang_combo=g_object_get_data(G_OBJECT(dialog), "curr_lang");
	GtkTreeModel *curr_lang_model=g_object_get_data(G_OBJECT(dialog), "curr_lang_model");

    if (response_id == GTK_RESPONSE_CANCEL)
    {
        gtk_widget_destroy(GTK_WIDGET(dialog));  // Close and destroy the dialog
    }
    else if(response_id == GTK_RESPONSE_OK)
    {
    	GtkWidget *profile_name=g_object_get_data(G_OBJECT(dialog), "profile_name");
		GtkWidget *lang_name=g_object_get_data(G_OBJECT(dialog), "lang_name");
		GtkWidget *bin_entry=g_object_get_data(G_OBJECT(dialog), "bin_entry");
		GtkWidget *bin_args_entry=g_object_get_data(G_OBJECT(dialog), "bin_args_entry");
		GtkWidget *search_entry=g_object_get_data(G_OBJECT(dialog), "search_entry");
		GtkTextBuffer *tbuffer=g_object_get_data(G_OBJECT(dialog), "lang_settings");
    
    	GObject *transf=g_object_get_data(G_OBJECT(dialog), "transf");
    	
    	xmlNode *xml_node=NULL;
    	xmlDoc *doc=NULL;
    	
    	//edit
    	if(transf)
    	{
    		xml_node=g_object_get_data(transf, "xml_node");
    	}
    	//new
    	else
    	{
			LspJumpConfigurationFile *conf=g_ptr_array_index(GLOBAL_LSPJUMP_CONFIGURATIONS,0);
			xmlNode *root=NULL;
			
			if(conf)
			{
				root = xmlDocGetRootElement(conf->doc);
			}
			else
			{
				doc = xmlNewDoc("1.0");
				root = xmlNewNode(NULL, "data");
				xmlDocSetRootElement(doc, root);
			}
			
			xml_node=xmlNewChild(root, NULL, "language", NULL);
    	}
    	
    	xmlSetProp(xml_node, "name", gtk_entry_get_text(GTK_ENTRY(profile_name)));
    	
    	xml_set_or_replace_child_content(xml_node,"lsp_language",gtk_entry_get_text(GTK_ENTRY(lang_name)));
		xml_set_or_replace_child_content(xml_node,"lsp_bin",gtk_entry_get_text(GTK_ENTRY(bin_entry)));
		xml_set_or_replace_child_content(xml_node,"lsp_bin_args",gtk_entry_get_text(GTK_ENTRY(bin_args_entry)));
		xml_set_or_replace_child_content(xml_node,"lsp_search",gtk_entry_get_text(GTK_ENTRY(search_entry)));
		
		GtkTextIter start, end;
		gtk_text_buffer_get_bounds(tbuffer, &start, &end);
		g_autofree char *lsp_settings_buf=gtk_text_buffer_get_text(tbuffer, &start, &end, FALSE);
		xml_set_or_replace_child_content(xml_node,"lsp_settings",lsp_settings_buf);
		
		//edit
    	if(transf)
    	{
			LspJumpConfigurationFile *file=g_object_get_data(transf, "xml_file");
			
			g_autofree char *file_path=g_file_get_path(file->file);
			
			if (xmlSaveFormatFileEnc(file_path, file->doc, "UTF-8", 1) == -1)
			{
				fprintf(stderr, "Failed to save updated XML to %s\n", file_path);
			}
			else
			{
				fprintf(stdout,"%s:%d Updated file [%s]\n",__FILE__,__LINE__,file_path);
				gtk_widget_destroy(GTK_WIDGET(dialog));
			}
			
			_update_language_combo_box(window,curr_lang_combo,curr_lang_model);
    	}
    	//new
    	else
    	{
			LspJumpConfigurationFile *conf=g_ptr_array_index(GLOBAL_LSPJUMP_CONFIGURATIONS,0);
			
			//use existing file
			if(conf)
			{
				g_autofree char *file_path=g_file_get_path(conf->file);
			
				if (xmlSaveFormatFileEnc(file_path, conf->doc, "UTF-8", 1) == -1)
				{
					fprintf(stderr, "Failed to save updated XML to %s\n", file_path);
				}
				else
				{
					fprintf(stdout,"%s:%d Updated file [%s]\n",__FILE__,__LINE__,file_path);
					gtk_widget_destroy(GTK_WIDGET(dialog));
				}
			}
			//create new file
			else
			{
				g_autofree char *dir_path = g_build_filename(g_get_home_dir(), ".config", "gedit", "lspjump", NULL);
				
				if (!g_file_test(dir_path, G_FILE_TEST_IS_DIR))
    			{
					if (g_mkdir_with_parents(dir_path, 0755) == 0)
					{
						printf("Directory created: %s\n", dir_path);
					}
					else
					{
						perror("Failed to create directory");
					}
				}
				
				g_autofree char *file_path = g_build_filename(g_get_home_dir(), ".config", "gedit", "lspjump", "lspjumpsettings.xml", NULL);
				
				if (xmlSaveFormatFileEnc(file_path, doc, "UTF-8", 1) == -1)
				{
					fprintf(stderr, "Failed to save updated XML to %s\n", file_path);
				}
				else
				{
					fprintf(stdout,"%s:%d Created file [%s]\n",__FILE__,__LINE__,file_path);
					gtk_widget_destroy(GTK_WIDGET(dialog));
				}
				
				LspJumpConfigurationFile *cfile=calloc(1,sizeof(LspJumpConfigurationFile));
				cfile->file=g_file_new_for_path(file_path);
				cfile->doc=doc;
				
				g_ptr_array_add(GLOBAL_LSPJUMP_CONFIGURATIONS,cfile);
			}
			
			_update_language_combo_box(window,curr_lang_combo,curr_lang_model);
    	}
    }
}

GtkWidget *create_language_settings_dialog(GeditWindow *window, const gchar *name, const gchar *LSP_LANGUAGES, const gchar *LSP_BIN, 
                                           const gchar *LSP_BIN_ARGS, const gchar *LSP_SEARCH_PATH, const gchar *LSP_SETTINGS, 
                                           GObject *transf, GtkWidget *lang_cb, GtkTreeModel *model)
{
	GtkWidget *dialog;
	GtkWidget *content_area;
	GtkWidget *label;

	dialog = gtk_dialog_new_with_buttons(
		"Language settings",
		GTK_WINDOW(window),
		GTK_DIALOG_MODAL,
		"_Cancel", GTK_RESPONSE_CANCEL,
		"_OK", GTK_RESPONSE_OK,
		NULL);
	
	g_signal_connect(dialog, "response", G_CALLBACK(on_create_language_settings_dialog_response), NULL);
	g_object_set_data(G_OBJECT(dialog), "transf",transf);
	
	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	label = gtk_label_new("Profile name:");
	gtk_box_pack_start(GTK_BOX(content_area), label, FALSE, FALSE, 5);

	GtkWidget *profile_name = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(profile_name), name);
	gtk_box_pack_start(GTK_BOX(content_area), profile_name, FALSE, FALSE, 5);

	label = gtk_label_new("Language names (comma separated):");
	gtk_box_pack_start(GTK_BOX(content_area), label, FALSE, FALSE, 5);

	GtkWidget *lang_name = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(lang_name), LSP_LANGUAGES);
	gtk_box_pack_start(GTK_BOX(content_area), lang_name, FALSE, FALSE, 5);

	label = gtk_label_new("Path to the LSP server binary:");
	gtk_box_pack_start(GTK_BOX(content_area), label, FALSE, FALSE, 5);

	GtkWidget *bin_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(bin_entry), LSP_BIN);
	gtk_box_pack_start(GTK_BOX(content_area), bin_entry, FALSE, FALSE, 5);

	label = gtk_label_new("Binary arguments (space separated):");
	gtk_box_pack_start(GTK_BOX(content_area), label, FALSE, FALSE, 5);

	GtkWidget *bin_args_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(bin_args_entry), LSP_BIN_ARGS);
	gtk_box_pack_start(GTK_BOX(content_area), bin_args_entry, FALSE, FALSE, 5);

	label = gtk_label_new("Search path to an eventual file or folder that may indicate where to start the binary from:");
	gtk_box_pack_start(GTK_BOX(content_area), label, FALSE, FALSE, 5);

	GtkWidget *search_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(search_entry), LSP_SEARCH_PATH);
	gtk_box_pack_start(GTK_BOX(content_area), search_entry, FALSE, FALSE, 5);

	label = gtk_label_new("Language settings:");
	gtk_box_pack_start(GTK_BOX(content_area), label, FALSE, FALSE, 5);
	
	GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_hexpand(scrolled_window, TRUE);
	gtk_widget_set_vexpand(scrolled_window, TRUE);
	gtk_widget_set_size_request(scrolled_window, -1, 100);
	
	GtkWidget *textview = gtk_text_view_new();
	GtkTextBuffer *tbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_buffer_set_text(tbuffer, LSP_SETTINGS, -1);
//	gtk_box_pack_start(GTK_BOX(content_area), textview, TRUE, TRUE, 5);
	
	gtk_container_add(GTK_CONTAINER(scrolled_window), textview);
	gtk_box_pack_start(GTK_BOX(content_area), scrolled_window, TRUE, TRUE, 5);
	
	g_object_set_data(G_OBJECT(dialog), "profile_name",profile_name);
	g_object_set_data(G_OBJECT(dialog), "lang_name",lang_name);
	g_object_set_data(G_OBJECT(dialog), "bin_entry",bin_entry);
	g_object_set_data(G_OBJECT(dialog), "bin_args_entry",bin_args_entry);
	g_object_set_data(G_OBJECT(dialog), "search_entry",search_entry);
	g_object_set_data(G_OBJECT(dialog), "lang_settings",tbuffer);
	g_object_set_data(G_OBJECT(dialog), "curr_lang", lang_cb);
	g_object_set_data(G_OBJECT(dialog), "curr_lang_model", model);

	gtk_widget_set_size_request(dialog, 700, 360);
	gtk_widget_show_all(dialog);

	return dialog;
}

static void _new_language(GtkWidget *widget, GeditWindow *window)
{
	GtkWidget *curr_lang_combo=g_object_get_data(G_OBJECT(widget), "curr_lang");
	GtkTreeModel *curr_lang_model=g_object_get_data(G_OBJECT(widget), "curr_lang_model");

	// Placeholder for creating new language
//	puts("New language creation: empty");
	
	const char *current_language=get_programming_language(window);
	
	create_language_settings_dialog(window,NULL,current_language,NULL,NULL,NULL,NULL,NULL,curr_lang_combo,curr_lang_model);
}

static void on_remove_language_response(GtkDialog *dialog, gint response_id, GtkWidget *widget)
{
    if (response_id == GTK_RESPONSE_OK)
    {
        GtkWidget *curr_lang_combo=g_object_get_data(G_OBJECT(widget), "curr_lang");
		GtkTreeModel *curr_lang_model=g_object_get_data(G_OBJECT(widget), "curr_lang_model");
		GeditWindow *window=g_object_get_data(G_OBJECT(dialog), "window");
		
		GtkTreeIter iter;
		
		if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(curr_lang_combo), &iter))
		{
			const char *column_name = NULL;
			GObject *obj = NULL;
			gtk_tree_model_get(curr_lang_model, &iter, COLUMN_TEXT, &column_name, COLUMN_OBJECT, &obj, -1);

			if (obj)
			{
				xmlNode *xml_node=g_object_get_data(obj, "xml_node");
				LspJumpConfigurationFile *file=g_object_get_data(obj, "xml_file");
				
				xmlUnlinkNode(xml_node);
				xmlFreeNode(xml_node);
				
				g_autofree char *file_path=g_file_get_path(file->file);
				
				if (xmlSaveFormatFileEnc(file_path, file->doc, "UTF-8", 1) == -1)
				{
					fprintf(stderr, "Failed to save updated XML to %s\n", file_path);
				}
				else
				{
					fprintf(stdout,"%s:%d Updated file [%s]\n",__FILE__,__LINE__,file_path);
					//gtk_list_store_remove(GTK_LIST_STORE(curr_lang_model), &iter);
					_update_language_combo_box(window,curr_lang_combo,curr_lang_model);
				}
			}
		}
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));  // Close the dialog
}

static void _remove_language(GtkWidget *widget, GeditWindow *window)
{
	GtkWidget *curr_lang_combo=g_object_get_data(G_OBJECT(widget), "curr_lang");
	GtkTreeModel *curr_lang_model=g_object_get_data(G_OBJECT(widget), "curr_lang_model");
	
	GtkTreeIter iter;
	
	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(curr_lang_combo), &iter))
	{
		const char *column_name = NULL;
		gtk_tree_model_get(curr_lang_model, &iter, COLUMN_TEXT, &column_name, -1);

		GtkWidget *dialog = gtk_dialog_new_with_buttons("Comfirmation", GTK_WINDOW(window),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, 
			"_OK", GTK_RESPONSE_OK, "_Cancel", GTK_RESPONSE_CANCEL, NULL);

		GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
		
		g_autofree char *labeltext=NULL;
		asprintf(&labeltext,"Are you sure you want to remove: %s",column_name);
		// Add a simple label
		GtkWidget *label = gtk_label_new(labeltext);
		gtk_box_pack_start(GTK_BOX(content_area), label, FALSE, FALSE, 5);
		
		g_object_set_data(G_OBJECT(dialog), "window", window);
		g_signal_connect(dialog, "response", G_CALLBACK(on_remove_language_response), widget);

		gtk_widget_show_all(dialog);
	}
}

static void _edit_language(GtkWidget *widget, GeditWindow *window)
{
	GtkWidget *curr_lang_combo=g_object_get_data(G_OBJECT(widget), "curr_lang");
	GtkTreeModel *curr_lang_model=g_object_get_data(G_OBJECT(widget), "curr_lang_model");
	
	GtkTreeIter iter;
	
	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(curr_lang_combo), &iter))
	{
		const char *column_name = NULL;
		GObject *obj = NULL;
		gtk_tree_model_get(curr_lang_model, &iter, COLUMN_TEXT, &column_name, COLUMN_OBJECT, &obj, -1);

		if (obj)
		{
			const char *lsp_language=g_object_get_data(obj, "lsp_language");
			const char *lsp_bin=g_object_get_data(obj, "lsp_bin");
			const char *lsp_bin_args=g_object_get_data(obj, "lsp_bin_args");
			const char *lsp_search=g_object_get_data(obj, "lsp_search");
			const char *lsp_settings=g_object_get_data(obj, "lsp_settings");
			xmlNode *xml_node=g_object_get_data(obj, "xml_node");
			LspJumpConfigurationFile *file=g_object_get_data(obj, "xml_file");

			fprintf(stdout,"%s:%d column_name [%s]\n",__FILE__,__LINE__,column_name);
			
			create_language_settings_dialog(window,column_name,lsp_language,lsp_bin,lsp_bin_args,lsp_search,lsp_settings,obj,curr_lang_combo,curr_lang_model);
			
			return;
		}
	}
}

static void _get_proj(GtkWidget *widget, GtkWidget *path_entry)
{
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

	puts("Get proj");
	
	GeditWindow *window=g_object_get_data(G_OBJECT(path_entry), "window-obj");

	GFile *gfile=lspjump_get_active_file_from_window(window);
	
	g_autoptr(GFile) parent_path=g_file_get_parent(gfile);
	
	g_autofree gchar *folder_path = find_parent_file_path(parent_path,target_filename);
	
	// Placeholder for getting project directory
	gtk_entry_set_text(GTK_ENTRY(path_entry), folder_path);
}

static void _click_histoy_path(GtkComboBox *combo_box, gpointer user_data)
{
	const char *new_path = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo_box));
	puts("empty");
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
//	gtk_entry_set_text(GTK_ENTRY(path_entry), GLOBAL_SETTINGS_OBJ.PROJECT_PATH);
	g_object_set_data(G_OBJECT(path_entry), "window-obj", window);
	gtk_grid_attach(GTK_GRID(grid), path_entry, 0, 1, 1, 1);
	_get_proj(NULL, path_entry);

	GtkWidget *change_button = gtk_button_new_with_label("Change");
	g_signal_connect(change_button, "clicked", G_CALLBACK(_change_project_path), path_entry);
	gtk_grid_attach(GTK_GRID(grid), change_button, 1, 1, 1, 1);

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
	GtkListStore *store = gtk_list_store_new(NUM_COLUMNS, G_TYPE_STRING, G_TYPE_OBJECT);
	
	lang_cb = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	_update_language_combo_box(window,lang_cb,GTK_TREE_MODEL(store));
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(lang_cb), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(lang_cb), renderer, "text", COLUMN_TEXT, NULL);
	gtk_grid_attach(GTK_GRID(grid), lang_cb, 0, 4, 2, 1);
	
	g_object_set_data(G_OBJECT(change_button), "curr_lang", lang_cb);
	g_object_set_data(G_OBJECT(change_button), "curr_lang_model", store);
	
	// Buttons for profile actions
	button = gtk_button_new_with_label("Edit");
	g_object_set_data(G_OBJECT(button), "curr_lang", lang_cb);
	g_object_set_data(G_OBJECT(button), "curr_lang_model", store);
	g_signal_connect(button, "clicked", G_CALLBACK(_edit_language), window);
	gtk_grid_attach(GTK_GRID(grid), button, 0, 5, 2, 1);

	button = gtk_button_new_with_label("New");
	g_object_set_data(G_OBJECT(button), "curr_lang", lang_cb);
	g_object_set_data(G_OBJECT(button), "curr_lang_model", store);
	g_signal_connect(button, "clicked", G_CALLBACK(_new_language), window);
	gtk_grid_attach(GTK_GRID(grid), button, 0, 6, 1, 1);

	button = gtk_button_new_with_label("Remove");
	g_object_set_data(G_OBJECT(button), "curr_lang", lang_cb);
	g_object_set_data(G_OBJECT(button), "curr_lang_model", store);
	g_signal_connect(button, "clicked", G_CALLBACK(_remove_language), window);
	gtk_grid_attach(GTK_GRID(grid), button, 1, 6, 1, 1);

	gtk_widget_show_all(dialog);
	return dialog;
}
