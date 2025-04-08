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
#include "gedit-lspjump-common.h"

GFile *lspjump_get_active_file_from_window(GeditWindow *window)
{
	GeditTab *active_tab = gedit_window_get_active_tab(window);
	if (active_tab == NULL) {
		g_print("No active tab found.\n");
		return NULL;
	}

	// Get the document from the active tab
	GeditDocument *document = gedit_tab_get_document(active_tab);
	if (document == NULL) {
		g_print("No document found in the active tab.\n");
		return NULL;
	}

	// Get the URI of the document
	GtkSourceFile *source_file = gedit_document_get_file(document);
	if (source_file == NULL) {
        g_print("No source file found.\n");
        return NULL;
    }
	
	return gtk_source_file_get_location(source_file);
}

char *get_full_text_from_active_document(GeditWindow *window)
{
	GeditTab *tab = gedit_window_get_active_tab(window);
	if (!tab)
	{
		g_print("No active tab.\n");
		return NULL;
	}

	GeditDocument *doc = gedit_tab_get_document(tab);
	GtkTextIter start, end;

	gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(doc), &start);
	gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(doc), &end);

	gchar *text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(doc), &start, &end, FALSE);

//	g_print("Document text:\n%s\n", text);
//	g_free(text);
	return text;
}

int gedit_lspjump_goto_file_line_column(GeditWindow *window, GFile *gfile, long line, long character)
{
	GeditTab *tab=gedit_window_get_tab_from_location(window,gfile);
	
	if(tab)
	{
		GeditDocument *doc = gedit_tab_get_document(tab);
	
		GtkTextBuffer *buffer = GTK_TEXT_BUFFER(doc);

		GtkTextIter iter;
		gtk_text_buffer_get_iter_at_line_offset(buffer, &iter, line, character);

		gtk_text_buffer_place_cursor(buffer, &iter);
		
		if (tab)
		{
			gedit_window_set_active_tab(window, tab);
			
			GeditView *view = gedit_tab_get_view(tab);
			GtkTextView *text_view = GTK_TEXT_VIEW(view);
			gtk_text_view_scroll_to_iter(text_view, &iter, 0.1, FALSE, 0.0, 0.0);
		}
	}
	else
	{
		GeditTab *tab=gedit_window_create_tab(window,TRUE);
		
		gedit_tab_load_file(tab,gfile,NULL,line+1,character+1,FALSE);

		if (!tab)
		{
			g_autofree char *uri=g_file_get_uri(gfile);
			g_warning("Failed to open file: %s", uri);
			return -1;
		}
	}
}
