/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#define PROTOCOL_VARS 6
#define STRING_VARS 5
#define RESPONSE_VARS 2

#define ADD_PROTOCOL_N(num) *last_p++ = htons(num);
#define ADD_PROTOCOL_(str, total) \
	*last_p++ = htons(str.len); *last_p_str++ = &str; total += str.len;
#define ADD_PROTOCOL(chk, str, total) \
	if(chk != NULL) { \
		ADD_PROTOCOL_(str, total)} \
	else {*last_p++ = 0; *last_p_str++ = NULL; }
#define ADD_RESPONSE_STR(num, total) num = ntohl(*last_p++); total += num;

typedef struct {
	ngx_http_upstream_conf_t upstream;
} ngx_http_webapp_loc_conf_t;

typedef struct {
	ngx_http_request_t* request;
	uint16_t remaining_header_len;
	uint32_t total_len;
	uint32_t content_type_len;
	uint32_t cookies_len;
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

