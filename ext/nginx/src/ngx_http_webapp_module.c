

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


static char *ngx_http_webapp(ngx_conf_t *cf, void *post, void *data);

static ngx_int_t ngx_http_webapp_init(ngx_cycle_t *cycle);
static ngx_int_t ngx_http_webapp_create_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_webapp_reinit_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_webapp_process_header(ngx_http_request_t *r);
static void ngx_http_webapp_abort_request(ngx_http_request_t *r);
static void ngx_http_webapp_finalize_request(ngx_http_request_t *r, ngx_int_t rc);
static char* ngx_http_webapp_conf(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

typedef struct {
	ngx_http_upstream_conf_t upstream;
} ngx_http_webapp_loc_conf_t;

static void* ngx_http_webapp_create_loc_conf(ngx_conf_t *cf) {
	ngx_http_webapp_loc_conf_t *conf;

	conf = (ngx_http_webapp_loc_conf_t*) ngx_pcalloc(cf->pool, sizeof(ngx_http_webapp_loc_conf_t));
	if (conf == NULL) {
		return NULL;
	}

	conf->upstream.connect_timeout = NGX_CONF_UNSET_MSEC;
	conf->upstream.send_timeout = NGX_CONF_UNSET_MSEC;
	conf->upstream.read_timeout = NGX_CONF_UNSET_MSEC;

	conf->upstream.buffer_size = NGX_CONF_UNSET_SIZE;

	conf->upstream.cyclic_temp_file = 0;
	conf->upstream.buffering = 0;
	conf->upstream.ignore_client_abort = 0;
	conf->upstream.send_lowat = 0;
	conf->upstream.bufs.num = 0;
	conf->upstream.busy_buffers_size = 0;
	conf->upstream.max_temp_file_size = 0;
	conf->upstream.temp_file_write_size = 0;
	conf->upstream.intercept_errors = 1;
	conf->upstream.intercept_404 = 1;
	conf->upstream.pass_request_headers = 0;
	conf->upstream.pass_request_body = 0;

	return conf;
}

static ngx_command_t ngx_http_webapp_commands[] = {
	{ ngx_string("webapp"),
	NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
	ngx_http_webapp_conf,
	NGX_HTTP_LOC_CONF_OFFSET,
	0,
	NULL },

	ngx_null_command
};


static ngx_http_module_t ngx_http_webapp_module_ctx = {
	NULL, /* preconfiguration */
	NULL, /* postconfiguration */

	NULL, /* create main configuration */
	NULL, /* init main configuration */

	NULL, /* create server configuration */
	NULL, /* merge server configuration */

	ngx_http_webapp_create_loc_conf, /* create location configuration */
	NULL /* merge location configuration */
};

ngx_module_t ngx_http_webapp_module = {
	NGX_MODULE_V1,
	&ngx_http_webapp_module_ctx, /* module context */
	ngx_http_webapp_commands, /* module directives */
	NGX_HTTP_MODULE, /* module type */
	NULL, /* init master */
	NULL, /* init module */
	ngx_http_webapp_init, /* init process */
	NULL, /* init thread */
	NULL, /* exit thread */
	NULL, /* exit process */
	NULL, /* exit master */
	NGX_MODULE_V1_PADDING
};

static ngx_int_t ngx_http_webapp_init(ngx_cycle_t *cycle) {

	return NGX_OK;
}


static ngx_int_t ngx_http_webapp_handler(ngx_http_request_t *r) {
	ngx_int_t                   rc;
	ngx_http_upstream_t        *u;
	ngx_http_webapp_loc_conf_t  *plcf;

	plcf = ngx_http_get_module_loc_conf(r, ngx_http_webapp_module);
	u = ngx_pcalloc(r->pool, sizeof(ngx_http_upstream_t));
	if (u == NULL) {
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	u->peer.log = r->connection->log;
	u->peer.log_error = NGX_ERROR_ERR;

	u->output.tag = (ngx_buf_tag_t) &ngx_http_webapp_module;

	u->conf = &plcf->upstream;

	/* attach the callback functions */
	u->create_request = ngx_http_webapp_create_request;
	u->reinit_request = ngx_http_webapp_reinit_request;
	u->process_header = ngx_http_webapp_process_header;
	u->abort_request = ngx_http_webapp_abort_request;
	u->finalize_request = ngx_http_webapp_finalize_request;

	r->upstream = u;

	rc = ngx_http_read_client_request_body(r, ngx_http_upstream_init);

	if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
		return rc;
	}

	return NGX_DONE;
}

/* Conf handler */
static char* ngx_http_webapp_conf(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
	ngx_http_webapp_loc_conf_t *lcf = conf;
	ngx_http_core_loc_conf_t   *clcf;
	ngx_str_t* value = cf->args->elts;
	ngx_url_t url;

	clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
	clcf->handler = ngx_http_webapp_handler;
	url.url = value[1];
	url.no_resolve = 1;
	lcf->upstream.upstream = ngx_http_upstream_add(cf, &url, 0);
	
	return NGX_CONF_OK;


}

/* Request definitions */

static ngx_int_t ngx_http_webapp_create_request(ngx_http_request_t *r) {
	return 0;
}

static ngx_int_t ngx_http_webapp_reinit_request(ngx_http_request_t *r) {
	return 0;
}

static ngx_int_t ngx_http_webapp_process_header(ngx_http_request_t *r) {
	return 0;
}

static void ngx_http_webapp_abort_request(ngx_http_request_t *r) {

}

static void ngx_http_webapp_finalize_request(ngx_http_request_t *r, ngx_int_t rc) {

}