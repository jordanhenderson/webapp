
/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

typedef struct {
    ngx_http_upstream_conf_t   upstream;
} ngx_http_webapp_loc_conf_t;


static char *ngx_http_webapp_pass(ngx_conf_t *cf, ngx_command_t *cmd,
								  void *conf);

//Location configuration functions
static void *ngx_http_webapp_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_webapp_merge_loc_conf(ngx_conf_t *cf, void *parent,
											void *child);

