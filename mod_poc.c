#include <http_core.h>
#include <httpd.h>
#include <http_config.h>

#include <apr_optional.h>
#include <ap_release.h>

#include <http_log.h>
#include <http_protocol.h>
#include <http_request.h>

// holder of prefetch http body data
typedef struct {
	apr_status_t cached_ret;
	apr_bucket_brigade *cached_brigade;
} poc_ctx_t;

// input filter helper for fetch and cache post body
static apr_status_t input_filter(ap_filter_t *f, apr_bucket_brigade *bb, ap_input_mode_t mode, apr_read_type_e block, apr_off_t readbytes) {
	poc_ctx_t *ctx = (poc_ctx_t*)f->ctx;
	if (ctx && ctx->cached_brigade) {
		APR_BRIGADE_CONCAT(bb, ctx->cached_brigade);
		apr_brigade_cleanup(ctx->cached_brigade);
		ctx->cached_brigade = NULL;
		return ctx->cached_ret;
	}
	return ap_get_brigade(f->next, bb, mode, block, readbytes);
}

// main handler
static int handler(request_rec* r) {

	//we don't want to skip processing subrequests
	//if ((r->main != NULL) || (r->prev != NULL)) {
	//	return DECLINED;
	//}
	// make sure it is fcgi or cgi handler (not the php one)
	if (ap_strstr_c(r->handler, "cgi") == NULL) {
		return DECLINED;
	}

	// initialize input filter context
	ap_filter_t *filter = ap_add_input_filter("poc_IN", NULL, r, r->connection);
	poc_ctx_t *ctx = (poc_ctx_t*)filter->ctx;
	if (!ctx) {
		filter->ctx = ctx = (poc_ctx_t*)apr_pcalloc(r->pool, sizeof(poc_ctx_t));
		ctx->cached_brigade = apr_brigade_create(filter->c->pool, filter->c->bucket_alloc);
	}

	// lets read the whole request body
	apr_bucket_brigade *bb = apr_brigade_create(filter->c->pool, filter->c->bucket_alloc);
	int fetch_more = 1;
	int found = 0;
	while (fetch_more) {
		ctx->cached_ret = ap_get_brigade(filter->next, bb, AP_MODE_READBYTES, APR_BLOCK_READ, HUGE_STRING_LEN);
		if (ctx->cached_ret != APR_SUCCESS) break;

		apr_bucket *b = APR_BRIGADE_FIRST(bb);
		APR_BRIGADE_CONCAT(ctx->cached_brigade, bb);
		for (; b != APR_BRIGADE_SENTINEL(ctx->cached_brigade); b = APR_BUCKET_NEXT(b)) {
			if (!fetch_more) break;

			if (APR_BUCKET_IS_EOS(b)) {
				fetch_more = 0;
				break;
			}

			if (APR_BUCKET_IS_METADATA(b)) continue;

			const char *buf;
			apr_size_t nbytes;
			if (apr_bucket_read(b, &buf, &nbytes, APR_BLOCK_READ) != APR_SUCCESS) continue;
			if (!nbytes) continue;

			if (memchr(buf, 'Z', nbytes)) {
				found = 1;
			}

			if (APR_BUCKET_NEXT(b) && APR_BUCKET_IS_EOS(APR_BUCKET_NEXT(b))) {
				fetch_more = 0;
			}
		} // for
	} // while (fetch_more)

	if (found) {
		static const char* page = "<html><body><h1>mod_poc handler</h1><p>The letter 'Z' was found!</p><p><a href=\"javascript:history.back();\">Go back</a></p></body></html>";
		ap_set_content_type(r, "text/html");
		ap_rwrite(page, strlen(page), r);
		return OK;
	}

	return DECLINED;
}

// entry point
static void register_hooks(apr_pool_t* pool) {
	ap_register_input_filter("poc_IN", input_filter, NULL, AP_FTYPE_RESOURCE);
	// be sure that our module is first in the chain
	static const char *const handlers_afterme_list[] = {
		"mod_php.c",
		"mod_cgi.c",
		"mod_cgid.c",
		"mod_fcgid.c",
		"mod_actions.c",
		NULL
	};
	ap_hook_handler(handler, NULL, handlers_afterme_list, APR_HOOK_REALLY_FIRST);
}

#ifdef APLOG_USE_MODULE
APLOG_USE_MODULE(poc);
#endif
module AP_MODULE_DECLARE_DATA poc_module = {
	STANDARD20_MODULE_STUFF,
	NULL,								/* create per-dir config structures		*/
	NULL,								/* merge  per-dir config structures		*/
	NULL,								/* create per-server config structures	*/
	NULL,								/* merge  per-server config structures	*/
	NULL,								/* table of config file commands		*/
	register_hooks						/* register hooks						*/
};
