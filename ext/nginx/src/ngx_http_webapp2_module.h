
/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#define PROTOCOL_VARS 6
#define STRING_VARS 5
#define INT_INTERVAL(i) sizeof(int)*i

typedef struct {
	ngx_http_upstream_conf_t upstream;
} ngx_http_webapp_loc_conf_t;

typedef struct {
	ngx_http_request_t* request;
} ngx_http_webapp_ctx_t;


static ngx_int_t ngx_http_webapp_create_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_webapp_reinit_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_webapp_process_header(ngx_http_request_t *r);
static ngx_int_t ngx_http_webapp_filter_init(void *data);
static ngx_int_t ngx_http_webapp_filter(void *data, ssize_t bytes);
static void ngx_http_webapp_abort_request(ngx_http_request_t *r);
static void ngx_http_webapp_finalize_request(ngx_http_request_t *r,
	ngx_int_t rc);

static char *ngx_http_webapp_pass(ngx_conf_t *cf, ngx_command_t *cmd,
								  void *conf);

//Location configuration functions
static void *ngx_http_webapp_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_webapp_merge_loc_conf(ngx_conf_t *cf, void *parent,
											void *child);

