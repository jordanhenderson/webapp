#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_http_webapp2_module.h"


ngx_module_t  ngx_http_webapp_module;

static ngx_conf_bitmask_t  ngx_http_webapp_next_upstream_masks[] = {
	{ ngx_string("error"), NGX_HTTP_UPSTREAM_FT_ERROR },
	{ ngx_string("timeout"), NGX_HTTP_UPSTREAM_FT_TIMEOUT },
	{ ngx_string("invalid_response"), NGX_HTTP_UPSTREAM_FT_INVALID_HEADER },
	{ ngx_string("not_found"), NGX_HTTP_UPSTREAM_FT_HTTP_404 },
	{ ngx_string("off"), NGX_HTTP_UPSTREAM_FT_OFF },
	{ ngx_null_string, 0 }
};

static ngx_command_t  ngx_http_webapp_commands[] = {
	{ ngx_string("webapp_pass"),
	  NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
	  ngx_http_webapp_pass,
	  NGX_HTTP_LOC_CONF_OFFSET,
	  0,
	  NULL },

	{ ngx_string("webapp_bind"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
	  ngx_http_upstream_bind_set_slot,
	  NGX_HTTP_LOC_CONF_OFFSET,
	  offsetof(ngx_http_webapp_loc_conf_t, upstream.local),
	  NULL },

	{ ngx_string("webapp_connect_timeout"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_msec_slot,
	  NGX_HTTP_LOC_CONF_OFFSET,
	  offsetof(ngx_http_webapp_loc_conf_t, upstream.connect_timeout),
	  NULL },

	{ ngx_string("webapp_send_timeout"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_msec_slot,
	  NGX_HTTP_LOC_CONF_OFFSET,
	  offsetof(ngx_http_webapp_loc_conf_t, upstream.send_timeout),
	  NULL },

	{ ngx_string("webapp_buffer_size"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_size_slot,
	  NGX_HTTP_LOC_CONF_OFFSET,
	  offsetof(ngx_http_webapp_loc_conf_t, upstream.buffer_size),
	  NULL },

	{ ngx_string("webapp_read_timeout"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_msec_slot,
	  NGX_HTTP_LOC_CONF_OFFSET,
	  offsetof(ngx_http_webapp_loc_conf_t, upstream.read_timeout),
	  NULL },

	{ ngx_string("webapp_next_upstream"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
	  ngx_conf_set_bitmask_slot,
	  NGX_HTTP_LOC_CONF_OFFSET,
	  offsetof(ngx_http_webapp_loc_conf_t, upstream.next_upstream),
	  &ngx_http_webapp_next_upstream_masks },

	ngx_null_command
};

static ngx_http_module_t  ngx_http_webapp_module_ctx = {
	NULL,                                  /* preconfiguration */
	NULL,                                  /* postconfiguration */

	NULL,                                  /* create main configuration */
	NULL,                                  /* init main configuration */

	NULL,                                  /* create server configuration */
	NULL,                                  /* merge server configuration */

	ngx_http_webapp_create_loc_conf,        /* create location configuration */
	ngx_http_webapp_merge_loc_conf          /* merge location configuration */
};

ngx_module_t  ngx_http_webapp_module = {
	NGX_MODULE_V1,
	&ngx_http_webapp_module_ctx,            /* module context */
	ngx_http_webapp_commands,               /* module directives */
	NGX_HTTP_MODULE,                       /* module type */
	NULL,                                  /* init master */
	NULL,                                  /* init module */
	NULL,                                  /* init process */
	NULL,                                  /* init thread */
	NULL,                                  /* exit thread */
	NULL,                                  /* exit process */
	NULL,                                  /* exit master */
	NGX_MODULE_V1_PADDING
};

//Handler functions
static ngx_int_t ngx_http_webapp_handler(ngx_http_request_t *r) {

}


//Location configuration functions
static void* ngx_http_webapp_create_loc_conf(ngx_conf_t *cf) {
	ngx_http_webapp_loc_conf_t* conf;
	conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_webapp_loc_conf_t));
	if(conf == NULL) return NULL; //pcalloc failed

	conf->upstream.local = NGX_CONF_UNSET_PTR;
	conf->upstream.connect_timeout = NGX_CONF_UNSET_MSEC;
	conf->upstream.send_timeout = NGX_CONF_UNSET_MSEC;
	conf->upstream.read_timeout = NGX_CONF_UNSET_MSEC;


	return conf;
}


static char* ngx_http_webapp_merge_loc_conf(ngx_conf_t *cf, void *parent,
											void *child) {
	ngx_http_webapp_loc_conf_t* prev = parent;
	ngx_http_webapp_loc_conf_t* conf = child;
	ngx_conf_merge_msec_value(conf->upstream.connect_timeout,
							  prev->upstream.connect_timeout, 60000);

	ngx_conf_merge_msec_value(conf->upstream.send_timeout,
							  prev->upstream.send_timeout, 60000);

	ngx_conf_merge_msec_value(conf->upstream.read_timeout,
							  prev->upstream.read_timeout, 60000);

	ngx_conf_merge_size_value(conf->upstream.buffer_size,
							  prev->upstream.buffer_size,
							  (size_t) ngx_pagesize);

	ngx_conf_merge_bitmask_value(conf->upstream.next_upstream,
							  prev->upstream.next_upstream,
							  (NGX_CONF_BITMASK_SET
							   |NGX_HTTP_UPSTREAM_FT_ERROR
							   |NGX_HTTP_UPSTREAM_FT_TIMEOUT));
	if (conf->upstream.next_upstream & NGX_HTTP_UPSTREAM_FT_OFF)
			conf->upstream.next_upstream =
					NGX_CONF_BITMASK_SET|NGX_HTTP_UPSTREAM_FT_OFF;

	if (conf->upstream.upstream == NULL)
		conf->upstream.upstream = prev->upstream.upstream;

	return NGX_CONF_OK;
}




/**
 * @brief ngx_http_webapp_pass handles initial webapp_pass directives.
 * @param cf The configuration object.
 * @param cmd
 * @param conf Pointer to the loc_conf_t object created for this module
 * @return status of success for nginx event handler
 */
static char* ngx_http_webapp_pass(ngx_conf_t *cf, ngx_command_t *cmd,
								  void *conf) {
	ngx_http_webapp_loc_conf_t* wlcf = conf;
	ngx_http_core_loc_conf_t* clcf;
	ngx_str_t* value;
	ngx_url_t u;

	if (wlcf->upstream.upstream) {
		return "is duplicate";
	}

	value = cf->args->elts;

	ngx_memzero(&u, sizeof(ngx_url_t));

	u.url = value[1];
	u.no_resolve = 1;

	wlcf->upstream.upstream = ngx_http_upstream_add(cf, &u, 0);
	if (wlcf->upstream.upstream == NULL) {
		return NGX_CONF_ERROR;
	}

	clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

	clcf->handler = ngx_http_webapp_handler;

	if (clcf->name.data[clcf->name.len - 1] == '/') {
		clcf->auto_redirect = 1;
	}

	return NGX_CONF_OK;
}
