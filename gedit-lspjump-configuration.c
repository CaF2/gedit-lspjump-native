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
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <dlfcn.h>
#include <unistd.h>

#include "gedit-lspjump-configuration.h"

GPtrArray *GLOBAL_LSPJUMP_CONFIGURATIONS = NULL;

static void parse_lspjump_file(const char *filepath);
static void process_lspjump(xmlNode *node);

void lspjump_configuration_file_free(LspJumpConfigurationFile *self)
{
	g_object_unref(self->file);
	xmlFreeDoc(self->doc);
	free(self);
}

char *get_so_directory()
{
	Dl_info info;
	if (dladdr((void *)get_so_directory, &info) == 0)
	{
		return NULL;
	}

	char *path = realpath(info.dli_fname, NULL);
	if (!path)
	{
		return NULL;
	}

	char *dir = strdup(dirname(path));
	free(path);
	return dir;
}

static void parse_lspjump_file(const char *filepath)
{
	xmlDoc *doc = xmlReadFile(filepath, NULL, 0);
	if (!doc)
	{
		return;
	}
	
	LspJumpConfigurationFile *cfile=calloc(1,sizeof(LspJumpConfigurationFile));
	cfile->file=g_file_new_for_path(filepath);
	cfile->doc=doc;
	
	g_ptr_array_add(GLOBAL_LSPJUMP_CONFIGURATIONS,cfile);
}

void exit_configuration(int exit_status, void *arg)
{
	g_ptr_array_free(GLOBAL_LSPJUMP_CONFIGURATIONS,TRUE);
}

int load_configuration()
{
	GLOBAL_LSPJUMP_CONFIGURATIONS = g_ptr_array_new_with_free_func((GDestroyNotify)lspjump_configuration_file_free);
	
	on_exit(exit_configuration,NULL);
	
	g_autofree char *home_config_dir = g_build_filename(g_get_home_dir(), ".config/gedit/lspjump/", NULL);
	
	g_autofree char *so_dir = get_so_directory();
	
	char *dirs[] = {so_dir, home_config_dir, "/usr/share/gedit/plugins/lspjump/", "/usr/local/share/gedit/plugins/lspjump/"};

	for (size_t i = 0; i < G_N_ELEMENTS(dirs); i++)
	{
		g_autoptr(GError) error = NULL;
		g_autoptr(GDir) dir = g_dir_open(dirs[i], 0, &error);
		if (!dir)
		{
			continue;
		}

		const gchar *filename;
		while ((filename = g_dir_read_name(dir)))
		{
			if (g_str_has_suffix(filename, ".xml"))
			{
				printf("Read configuration file: %s\n",filename);
			
				char *filepath = g_build_filename(dirs[i], filename, NULL);
				parse_lspjump_file(filepath);
				g_free(filepath);
			}
		}
	}

	return 0;
}

xmlNode *xml_get_child_by_tag(xmlNode *parent, const char *const tag)
{
	for (xmlNode *child = parent->children; child; child = child->next)
	{
		if (child->type == XML_ELEMENT_NODE && xmlStrcmp(child->name, (const xmlChar *)tag) == 0)
		{
			return child; // caller must free
		}
	}
	return NULL;
}
