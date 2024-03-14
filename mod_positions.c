/* Microservice that sends positions data */

#include <string.h>
#include <httpd.h>
#include <http_protocol.h>
#include <http_config.h>
#include "ap_config.h"
#include "apr_buckets.h"
#include "apr_strings.h"

static int positions_handler(request_rec *request) {
	if (!request->handler || strcmp(request->handler, "positions")) {
		return DECLINED;
	}

	if (request->method_number != M_GET) {
		return HTTP_METHOD_NOT_ALLOWED;
	}
	
	// Assuming the base URL is /positions and we want to strip this from the request URI
	const char *base_path = "/positions";
	const char *request_path = request->uri;
	// Check if  the request URI starts with the base path
	if (strncmp(request_path, base_path, strlen(base_path)) == 0) {
		// Adjust the pointer to skip the the base path
		request_path += strlen(base_path); // TODO controlla che strlen(base_path) abbia un limite accettabile
	}
	else {
		// The request URI does not start with the expected base path
		return HTTP_NOT_FOUND;
	} 
	
	// At this point, request_path should contain the directory
	request->content_type = "application/json";
	
	// Subpool for memory management
	apr_pool_t *pool;
	apr_dir_t *dir;
	apr_finfo_t dirent;
	apr_status_t rv;
	
	apr_pool_create(&pool, request->pool);
	
	// Open the directory
	rv = apr_dir_open(&dir, request_path, pool);
	if (rv != APR_SUCCESS) {
		// Handle error
		return HTTP_NOT_FOUND;
	}
	
	// Use a bucket brigade to buffer output
	apr_bucket_brigade *bb = apr_brigade_create(pool, request->connection->bucket_alloc);
	apr_brigade_printf(bb, NULL, NULL, "[");
	
	int first = 1;
	while (apr_dir_read(&dirent, APR_FINFO_NAME, dir) == APR_SUCCESS) {
		// Skip "." and ".." directories
		if (strcmp(dirent.name, ".") != 0 && strcmp(dirent.name, "..") != 0 && dirent.filetype == APR_DIR) {
			if (first) {
				first = 0;
			}
			else {
				apr_brigade_printf(bb, NULL, NULL, ", ");
			}
						
			// Append the directory name to the buffer
			apr_brigade_printf(bb, NULL, NULL, "\"%s\"", dirent.name);
		}
	}
	
	apr_brigade_printf(bb, NULL, NULL, "]");
	
	// Calculate the length of the brigade content
	apr_size_t len = 0;
	for (apr_bucket *b = APR_BRIGADE_FIRST(bb); b != APR_BRIGADE_SENTINEL(bb); b = APR_BUCKET_NEXT(b)) {
		if (!APR_BUCKET_IS_METADATA(b)) {
			const char *data;
			apr_size_t data_len;
			apr_bucket_read(b, &data, &data_len, APR_BLOCK_READ);
			len += data_len;
		}
	}
	
	// Set the Content-Length header
	apr_table_setn(request->headers_out, "Content-Length", apr_itoa(request->pool, len));
	
	// Send the brigade content
	ap_pass_brigade(request->output_filters, bb);
	
	// Close the directory and destroy the pool
	apr_dir_close(dir);
	apr_pool_destroy(pool);
	
	/* ap_set_content_type(request, "text/html;charset=ascii");
	ap_rputs("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\">\n", request);
    ap_rputs("<html><head><title>Apache Positions Module</title></head>", request);
    ap_rputs("<body><h1>Positions</h1>", request);
    ap_rputs("<p>This is the Positions module!</p>", request);
    ap_rputs("</body></html>", request);
	*/
	
	// ap_rputs(request_path, request);

	return OK;
}

static void positions_hooks(apr_pool_t *pool) {
    ap_hook_handler(positions_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

module AP_MODULE_DECLARE_DATA positions_module = {
        STANDARD20_MODULE_STUFF,
        NULL, // create per-dir config structures
        NULL, // merge per-dir config structures
        NULL, // create per-server config structures
        NULL, // merge per-server config structures
        NULL, // table of config file commands
        positions_hooks // register hooks
};
