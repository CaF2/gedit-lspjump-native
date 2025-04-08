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
#pragma once

#include <glib.h>
#include <jansson.h>
#include <gedit/gedit-window.h>
#include <gedit/gedit-document.h>

G_BEGIN_DECLS

extern GQueue *GLOBAL_BACK_STACK;
extern GQueue *GLOBAL_FORWARD_STACK;

typedef struct TrackPos
{
	GFile *file;
	long line;
	long character;
}TrackPos;

GFile *lspjump_get_active_file_from_window(GeditWindow *window);
char *get_full_text_from_active_document(GeditWindow *window);
int gedit_lspjump_goto_file_line_column(GeditWindow *window, GFile *gfile, long line, long character);
int gedit_lspjump_goto_file_line_column_and_track(GeditWindow *window, GFile *gfile, long line, long character, GQueue *stack);
int gedit_lspjump_do_undo(GeditWindow *window);
int gedit_lspjump_do_redo(GeditWindow *window);

G_END_DECLS
