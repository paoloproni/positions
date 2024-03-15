/* Microservice that sends a JSon with the geographical hierarchy of an italian city which has the given CAP */

#include <string.h>
#include <httpd.h>
#include "http_log.h"
#include <http_protocol.h>
#include <http_config.h>
#include "ap_config.h"
#include "apr_buckets.h"
#include "apr_strings.h"
#include <unistd.h> // For readlink
#include <limits.h> // For PATH_MAX

static int positions_handler(request_rec *request) {
	if (!request->handler || strcmp(request->handler, "italiancaps")) {
		return DECLINED;
	}

	if (request->method_number != M_GET) {
		return HTTP_METHOD_NOT_ALLOWED;
	}
	
	ap_log_rerror(__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_NOTICE, APR_SUCCESS, request, "italiancaps is processing a request");
	
	// Assuming the base URL is /positions and we want to strip this from the request URI
	const char *base_path = "/italiancaps";
	const char *request_cap = request->uri;
	// Check if  the request URI starts with the positions
	if (strncmp(request_cap, base_path, strlen(base_path)) == 0) {
		// Adjust the pointer to skip the the base path
		request_cap += strlen(base_path);
	}
	else {
		// The request URI does not start with the expected base path
		return HTTP_NOT_FOUND;
	}

	// Check for directory traversal attempts
	if (strstr(request_cap, "..") != NULL) {
		// Log the attempt, respond with an error, or handle as appropriate
		ap_log_rerror(__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_NOTICE, APR_SUCCESS, request, "The URL contains ..");
		return HTTP_FORBIDDEN;
	}

	// At this point, request_path should contain the CAP. Construct the full path to the symbolic link
	const char *dir = "/cap";
	char *symlink_path = apr_psprintf(request->pool, "%s/%s", dir, request_cap);
	
	// Buffer to store the resolved path
	char target_path[PATH_MAX];
	
	// Attempt to resolve the symbolic link
	ssize_t len = readlink(symlink_path, target_path, sizeof(target_path) - 1);
	if (len <= 0) {
		// The file is not a symbolic link or another error occurred
		return HTTP_NOT_FOUND;
	}
	target_path[len] = '\0'; // Null terminate the result
	const char * treeBasePath = "/tree";
	const int treeLen = strlen(treeBasePath);
	if (len > treeLen) {
		char * adjusted_path = target_path + treeLen;
		// Set the content type and send the resolved path as the response
		ap_set_content_type(request, "text/plain");
		ap_rputs(adjusted_path, request);
	}
	else {
		ap_log_rerror(__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_NOTICE, APR_SUCCESS, request, "The len is smaller that 5, is the base directory different from /tree ?");
		return HTTP_NOT_FOUND;
	}
	
	return OK;
}

static void italiancaps_hooks(apr_pool_t *pool) {
    ap_hook_handler(positions_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

module AP_MODULE_DECLARE_DATA italiancaps_module = {
        STANDARD20_MODULE_STUFF,
        NULL, // create per-dir config structures
        NULL, // merge per-dir config structures
        NULL, // create per-server config structures
        NULL, // merge per-server config structures
        NULL, // table of config file commands
        italiancaps_hooks // register hooks
};
