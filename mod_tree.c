/* Microservice that sends a position tree from the given path */

#include <string.h>
#include <httpd.h>
#include <http_log.h>
#include <http_protocol.h>
#include <http_config.h>
#include <ap_config.h>
#include "apr_buckets.h"
#include "apr_strings.h"
#include "apr_file_info.h"
#include "apr_file_io.h"
#include "apr_errno.h"
#include <json-c/json.h>

// Prototype for the directory traversal function
static void traverse_directory(apr_pool_t *pool, json_object *jobj, const char *path, const char *prefix, const request_rec *request);

// Function to traverse directories and add to JSON object
static void traverse_directory(apr_pool_t *pool, json_object *jobj, const char *path, const char *prefix, const request_rec *request) {
	apr_dir_t *dir;
	apr_status_t rv;
	apr_finfo_t dirent;
	
	ap_log_rerror(__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_NOTICE, APR_SUCCESS, request, "I have to open %s", path);
	rv = apr_dir_open(&dir, path, pool);
	
	if (rv != APR_SUCCESS) {
		ap_log_rerror(__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_NOTICE, APR_SUCCESS, request, "Impossibile to open the base directory");
		return;
	}
	
	while (apr_dir_read(&dirent, APR_FINFO_NAME | APR_FINFO_TYPE, dir) == APR_SUCCESS) {
		if (dirent.filetype == APR_DIR) {
			// Skip "." and ".." directories
			if (strcmp(dirent.name, ".") == 0 || strcmp(dirent.name, "..") == 0) continue;
			
			json_object *sub_jobj = json_object_new_object(); // Create a new JSON object for this directory
			
			char *new_prefix = apr_pstrcat(pool, prefix, dirent.name, "/", NULL, request);
			json_object_object_add(jobj, dirent.name, sub_jobj); // Add the new directory JSON object to the parent
			
			// Recursively traverse subdirectories
			traverse_directory(pool, sub_jobj, apr_pstrcat(pool, path, "/", dirent.name, NULL), new_prefix, request);
		}
	}
	
	apr_dir_close(dir);
}

static int tree_handler(request_rec *request) {
	if (!request->handler || strcmp(request->handler, "tree")) {
		return DECLINED;
	}

	if (request->method_number != M_GET) {
		return HTTP_METHOD_NOT_ALLOWED;
	}
	
	ap_log_rerror(__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_NOTICE, APR_SUCCESS, request, "tree is processing a request");
	
	// Assuming the base URL is /positions and we want to strip this from the request URI
	const char *base_path = "/tree";
	const char *request_tree = request->uri;
	// Check if  the request URI starts with the positions
	if (strncmp(request_tree, base_path, strlen(base_path)) == 0) {
		// Adjust the pointer to skip the the base path
		request_tree += strlen(base_path);
		// Now request_path does not contain /tree anymore
	}
	else {
		// The request URI does not start with the expected base path
		return HTTP_NOT_FOUND;
	}
	// Check for directory traversal attempts
	if (strstr(request_tree, "..") != NULL) {
		// Log the attempt, respond with an error, or handle as appropriate
		ap_log_rerror(__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_NOTICE, APR_SUCCESS, request, "The URL contains ..");
		return HTTP_FORBIDDEN;
	}
	
	// Kick off the directory traversal from the request URI
	const char *base_real_path = "/tree";
	const char *request_real_path = apr_pstrcat(request->pool, base_real_path, request_tree, NULL);
	
	json_object *response_jobj = json_object_new_object(); // Create the root JSON object
	
	traverse_directory(request->pool, response_jobj, request_real_path, "", request);
	
	// Convert the JSON object to a string
	const char *response_str = json_object_to_json_string_ext(response_jobj, JSON_C_TO_STRING_PRETTY);
	
	ap_set_content_type(request, "application/json");
	ap_rputs(response_str, request);
	
	json_object_put(response_jobj); // Free the JSON object
	
	return OK;
}

static void tree_hooks(apr_pool_t *pool) {
    ap_hook_handler(tree_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

module AP_MODULE_DECLARE_DATA tree_module = {
        STANDARD20_MODULE_STUFF,
        NULL, // create per-dir config structures
        NULL, // merge per-dir config structures
        NULL, // create per-server config structures
        NULL, // merge per-server config structures
        NULL, // table of config file commands
        tree_hooks // register hooks
};
