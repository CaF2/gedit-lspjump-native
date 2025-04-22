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
#include <stdint.h>

G_BEGIN_DECLS

typedef struct JsonRpcEndpoint JsonRpcEndpoint;
typedef void (*IdActionFunction)(JsonRpcEndpoint *endpoint, json_t *root, void *user_data);

typedef struct RpcIdAction
{
	int id;
	IdActionFunction action;
	void *user_data;
	
	uint8_t active;
}RpcIdAction;

#define GEDIT_RPC_ID_ACTIONS_LEN 64

struct JsonRpcEndpoint
{
	GIOChannel *stdin_channel;
	GIOChannel *stdout_channel;
	GIOChannel *stderr_channel;
	GPid child_pid;
	GString *read_buffer;
	
	RpcIdAction id_actions[GEDIT_RPC_ID_ACTIONS_LEN];
	
	uint8_t initialized: 1;
};

int lspjump_rpc_init(const char *const root_uri,const char *const lsp_bin,const char *const lsp_bin_args,const char *const lsp_settings);

int lspjump_rpc_definition(const char *const file_path, const char *const file_contents, long doc_line, long doc_offset,
                           IdActionFunction action, void *user_data);

int lspjump_rpc_reference(const char *const file_path, const char *const file_contents, long doc_line, long doc_offset,
                           IdActionFunction action, void *user_data);

int lspjump_rpc_hover(const char *const file_path, const char *const file_contents, long doc_line, long doc_offset,
                      IdActionFunction action, void *user_data);

G_END_DECLS
