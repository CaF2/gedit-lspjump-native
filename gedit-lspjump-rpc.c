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
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "gedit-lspjump-rpc.h"

int GLOBAL_RPC_ID=1;

static JsonRpcEndpoint *GLOBAL_ENDPOINT=NULL;

static void send_request(JsonRpcEndpoint *endpoint, const char *message)
{
	GError *error = NULL;
	gsize bytes_written;
	g_io_channel_write_chars(endpoint->stdin_channel, message, -1, &bytes_written, &error);
	g_io_channel_flush(endpoint->stdin_channel, &error);
	if (error)
	{
		g_printerr("Error writing to child: %s\n", error->message);
		g_error_free(error);
	}
}

int store_rpc_action(JsonRpcEndpoint *endpoint, IdActionFunction action, void *user_data)
{
	for(int i=0;i<GEDIT_RPC_ID_ACTIONS_LEN;i++)
	{
		if(endpoint->id_actions[i].id==0)
		{
			endpoint->id_actions[i].id=GLOBAL_RPC_ID;
			endpoint->id_actions[i].action=action;
			endpoint->id_actions[i].user_data=user_data;
			
			return GLOBAL_RPC_ID;
		}
	}
	
	return -1;
}

/**
	@param id
		id>=0 send that id
		id==-1 send with next id
		id==-2 send with no id, needed for initialized
*/
int send_rpc_message(JsonRpcEndpoint *endpoint, const char *const method_name, json_t *params, long id)
{
	long use_id=-3;

	json_t *root = json_pack("{s:s, s:s, s:O}",
		"jsonrpc", "2.0",
		"method", method_name,
		"params", params?params:json_object()
	);
	
	if(id>=-1)
	{
		use_id=(id>=0)?id:(GLOBAL_RPC_ID++);
		json_object_set_new(root, "id", json_integer(use_id));
	}
	
//	if(params)
//	{
//		json_object_set_new(root, "params", params);
//	}
	
	g_autofree char *json_str = json_dumps(root, JSON_COMPACT);
	
	fprintf(stdout,"%s:%d SEND RPC STRING: [%s]\n",__FILE__,__LINE__,json_str);
	
	g_autofree char *send_msg=NULL;
	
	asprintf(&send_msg,"Content-Length: %ld\r\n\r\n%s",strlen(json_str),json_str);
	
	send_request(endpoint, send_msg);
	
	return use_id;
}

static gboolean read_stdout(GIOChannel *source, GIOCondition condition, gpointer data)
{
	JsonRpcEndpoint *endpoint = (JsonRpcEndpoint *)data;
	GError *error = NULL;
	gchar buffer[4096];
	gsize bytes_read=0;

	if (condition & G_IO_HUP) {
		g_print("Child process closed stdout.\n");
		return FALSE;
	}

	while (g_io_channel_read_chars(endpoint->stdout_channel, buffer, sizeof(buffer) - 1, &bytes_read, &error) == G_IO_STATUS_NORMAL && bytes_read > 0) {
		buffer[bytes_read] = '\0';
		g_string_append(endpoint->read_buffer, buffer);

		// Look for complete LSP messages
		while (1) {
			// Look for Content-Length header
			char *header_end = strstr(endpoint->read_buffer->str, "\r\n\r\n");
			if (!header_end)
				break;

			size_t header_len = header_end - endpoint->read_buffer->str + 4;
			char *content_length_str = g_strstr_len(endpoint->read_buffer->str, header_len, "Content-Length:");

			if (!content_length_str)
				break;

			int content_length = 0;
			sscanf(content_length_str, "Content-Length: %d", &content_length);

			if (endpoint->read_buffer->len < header_len + content_length)
				break; // not enough data yet

			// Extract JSON message
			char *json_start = endpoint->read_buffer->str + header_len;
			char *json_chunk = g_strndup(json_start, content_length);

			// Parse JSON
			json_error_t jerr;
			json_t *json = json_loads(json_chunk, 0, &jerr);

			if (json)
			{
				// Print parsed result (or handle LSP responses)
				g_autofree gchar *formatted = json_dumps(json, JSON_INDENT(2));
				g_print("Parsed JSON:\n%s\n", formatted);

				// Check for "id":0 which is response to initialize
				json_t *id = json_object_get(json, "id");
				json_int_t id_val=json_integer_value(id);
				if (id && json_is_integer(id))
				{
					for(int i=0;i<GEDIT_RPC_ID_ACTIONS_LEN;i++)
					{
						if(endpoint->id_actions[i].id==id_val)
						{
							endpoint->id_actions[i].action(endpoint,json,endpoint->id_actions[i].user_data);
						
							endpoint->id_actions[i].id=0;
							endpoint->id_actions[i].action=NULL;
							endpoint->id_actions[i].user_data=NULL;
							
							break;
						}
					}
				}

				json_decref(json);
			}
			else {
				g_printerr("JSON parse error: %s at line %d\n", jerr.text, jerr.line);
			}

			g_free(json_chunk);

			// Remove the processed message
			g_string_erase(endpoint->read_buffer, 0, header_len + content_length);
		}
	}

	if (error) {
		g_error_free(error);
	}

	return TRUE;
}


static gboolean read_stderr(GIOChannel *source, GIOCondition condition, gpointer data) {
	JsonRpcEndpoint *endpoint = (JsonRpcEndpoint *)data;
	gchar *message = NULL;
	gsize len;
	GError *error = NULL;

	if (condition & G_IO_HUP) {
		g_print("Child process closed stderr.\n");
		return FALSE;
	}

	if (g_io_channel_read_line(endpoint->stderr_channel, &message, &len, NULL, &error) == G_IO_STATUS_NORMAL) {
		g_print("ERR:: %s", message);
		g_free(message);
	}

	if (error) {
		g_error_free(error);
	}

	return TRUE;
}

static void spawn_child(JsonRpcEndpoint *endpoint, const gchar *program, gchar **args) {
	GError *error = NULL;
	gint stdin_fd, stdout_fd, stderr_fd;

	if (!g_spawn_async_with_pipes(NULL, args, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &endpoint->child_pid, &stdin_fd, &stdout_fd, &stderr_fd, &error)) {
		g_printerr("Failed to spawn process: %s\n", error->message);
		g_error_free(error);
		return;
	}
	
	endpoint->read_buffer = g_string_new(NULL);
	
	endpoint->stdin_channel = g_io_channel_unix_new(stdin_fd);
	g_io_channel_set_flags(endpoint->stdin_channel, G_IO_FLAG_NONBLOCK, NULL);
	endpoint->stdout_channel = g_io_channel_unix_new(stdout_fd);
//	g_io_channel_set_encoding(endpoint->stdout_channel, NULL, NULL);
	g_io_channel_set_flags(endpoint->stdout_channel, G_IO_FLAG_NONBLOCK, NULL);
//	g_io_channel_set_buffered(endpoint->stdout_channel, FALSE);
	endpoint->stderr_channel = g_io_channel_unix_new(stderr_fd);
	g_io_channel_set_flags(endpoint->stderr_channel, G_IO_FLAG_NONBLOCK, NULL);

	g_io_add_watch(endpoint->stdout_channel, G_IO_IN | G_IO_HUP, read_stdout, endpoint);
	g_io_add_watch(endpoint->stderr_channel, G_IO_IN | G_IO_HUP, read_stderr, endpoint);
}

const char *LOGIN_STR="{"
"	\"textDocument\": {\"codeAction\": {\"dynamicRegistration\": true},"
"	\"codeLens\": {\"dynamicRegistration\": true},"
"	\"colorProvider\": {\"dynamicRegistration\": true},"
"	\"completion\": {\"completionItem\": {\"commitCharactersSupport\": true,\"documentationFormat\": [\"markdown\", \"plaintext\"],\"snippetSupport\": true},"
"	\"completionItemKind\": {\"valueSet\": [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25]},"
"	\"contextSupport\": true,"
"	\"dynamicRegistration\": true},"
"	\"definition\": {\"dynamicRegistration\": true},"
"	\"documentHighlight\": {\"dynamicRegistration\": true},"
"	\"documentLink\": {\"dynamicRegistration\": true},"
"	\"documentSymbol\": {\"dynamicRegistration\": true,"
"	\"symbolKind\": {\"valueSet\": [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26]}},"
"	\"formatting\": {\"dynamicRegistration\": true},"
"	\"hover\": {\"contentFormat\": [\"markdown\", \"plaintext\"],"
"	\"dynamicRegistration\": true},"
"	\"implementation\": {\"dynamicRegistration\": true},"
"	\"onTypeFormatting\": {\"dynamicRegistration\": true},"
"	\"publishDiagnostics\": {\"relatedInformation\": true},"
"	\"rangeFormatting\": {\"dynamicRegistration\": true},"
"	\"references\": {\"dynamicRegistration\": true},"
"	\"rename\": {\"dynamicRegistration\": true},"
"	\"signatureHelp\": {\"dynamicRegistration\": true,"
"	\"signatureInformation\": {\"documentationFormat\": [\"markdown\", \"plaintext\"]}},"
"	\"synchronization\": {\"didSave\": true,"
"	\"dynamicRegistration\": true,"
"	\"willSave\": true,"
"	\"willSaveWaitUntil\": true},"
"	\"typeDefinition\": {\"dynamicRegistration\": true}},"
"	\"workspace\": {\"applyEdit\": true,"
"	\"configuration\": true,"
"	\"didChangeConfiguration\": {\"dynamicRegistration\": true},"
"	\"didChangeWatchedFiles\": {\"dynamicRegistration\": true},"
"	\"executeCommand\": {\"dynamicRegistration\": true},"
"	\"symbol\": {\"dynamicRegistration\": true,"
"	\"symbolKind\": {\"valueSet\": [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26]}},\"workspaceEdit\": {\"documentChanges\": true},"
"	\"workspaceFolders\": true}"
"}";

//const char *LOGIN_STR="{}";

static void init_cb(JsonRpcEndpoint *endpoint, json_t *root, void *user_data)
{
	g_print("Initialize response received\n");

	// Send "initialized"
	send_rpc_message(endpoint, "initialized", NULL, -2);
	
	endpoint->initialized=1;
}

int initialize(JsonRpcEndpoint *endpoint,const char *const root_path, const char *const root_uri, json_t *initialization_options,
               json_t *capabilities, const char *const trace, json_t *workspace_folders)
{
	g_autofree char *json_str = json_dumps(capabilities, JSON_COMPACT);
	
//	pid_t process_id = getpid();
	pid_t process_id = endpoint->child_pid;
	
	json_t *params = json_pack("{s:i, s:s, s:O, s:s, s:O}",
		"processId",process_id,
		"rootUri",root_uri,
		"capabilities",capabilities,
		"trace",trace,
		"workspaceFolders",workspace_folders
	);
	
	if(root_path)
	{
		json_object_set_new(params, "rootPath", json_string(root_path));
	}
	
	if(initialization_options)
	{
		json_object_set_new(params, "initializationOptions", initialization_options);
	}
	
	int send_id=store_rpc_action(endpoint,init_cb,NULL);
	
	send_rpc_message(endpoint,"initialize",params,send_id);
}

int lspjump_rpc_definition(const char *const file_path, const char *const file_contents, long doc_line, long doc_offset,
                           IdActionFunction action, void *user_data)
{
	JsonRpcEndpoint *endpoint=GLOBAL_ENDPOINT;
	
	if(endpoint)
	{
		g_autofree char *uri_path=NULL;
		asprintf(&uri_path,"file://%s",file_path);
		
		json_t *params = json_pack("{s:{s:s, s:s, s:i, s:s}}",
			"textDocument",
			"uri", uri_path,
			"languageId", "c",
			"version", 1,
			"text", file_contents
		);
		
		send_rpc_message(endpoint,"textDocument/didOpen",params,-2);
		
		json_t *params2 = json_pack("{s:{s:s},s:{s:i,s:i}}",
			"textDocument",
			"uri", uri_path,
			"position",
			"line",doc_line,
			"character",doc_offset
		);

		int send_id=store_rpc_action(endpoint,action,user_data);

		send_rpc_message(endpoint,"textDocument/definition",params2,-1);

		return 0;
	}
	
	return 1;
}

int lspjump_rpc_reference(const char *const file_path, const char *const file_contents, long doc_line, long doc_offset,
                           IdActionFunction action, void *user_data)
{
	JsonRpcEndpoint *endpoint=GLOBAL_ENDPOINT;
	
	if(endpoint)
	{
		g_autofree char *uri_path=NULL;
		asprintf(&uri_path,"file://%s",file_path);
		
		json_t *params = json_pack("{s:{s:s, s:s, s:i, s:s}}",
			"textDocument",
			"uri", uri_path,
			"languageId", "c",
			"version", 1,
			"text", file_contents
		);
		
		send_rpc_message(endpoint,"textDocument/didOpen",params,-2);
		
		json_t *params2 = json_pack("{s:{s:s},s:{s:i,s:i}}",
			"textDocument",
			"uri", uri_path,
			"position",
			"line",doc_line,
			"character",doc_offset
		);

		int send_id=store_rpc_action(endpoint,action,user_data);

		send_rpc_message(endpoint,"textDocument/references",params2,-1);

		return 0;
	}
	
	return 1;
}

int lspjump_rpc_init(const char *const root_uri,const char *const lsp_bin,const char *const lsp_bin_args,const char *const lsp_settings)
{
	GLOBAL_ENDPOINT=calloc(1,sizeof(JsonRpcEndpoint));
	
	g_auto(GStrv) bin_args = g_strsplit(lsp_bin_args, " ", -1);
	
	guint arg_len=bin_args?g_strv_length(bin_args):0;
	
	gchar *args[arg_len+2];
	args[0]=lsp_bin?lsp_bin:"/usr/bin/clangd";
	int i=0;
	while(i<arg_len)
	{
		args[i+1]=bin_args[i];
		i++;
	}
	args[i+1]=NULL;
	spawn_child(GLOBAL_ENDPOINT, args[0], args);
	
//	const char *const root_uri="file:///home/flev/dev/c/test/lsp_c";
	
	json_error_t error;
	json_t *capabilities = json_loads(lsp_settings?lsp_settings:LOGIN_STR, 0, &error);
	
	if (!capabilities)
	{
		fprintf(stderr, "JSON parse error on line %d: %s\n", error.line, error.text);
		return 1;
	}
	
	json_t *workspace_folders = json_pack("[{s:s, s:s}]",
		"name", "gedit-lspjump-plugin",
		"uri", root_uri
	);
	
	initialize(GLOBAL_ENDPOINT,NULL,root_uri,NULL,capabilities,"off",workspace_folders);

	return 0;
}
