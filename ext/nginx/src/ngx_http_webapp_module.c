#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    ngx_str_t                      host_header;
    ngx_str_t                      port;
    ngx_str_t                      uri;
} ngx_http_webapp_vars_t;


typedef struct {
    ngx_http_upstream_conf_t       upstream;

    ngx_array_t                   *flushes;
    ngx_array_t                   *body_set_len;
    ngx_array_t                   *body_set;
    ngx_array_t                   *headers_set_len;
    ngx_array_t                   *headers_set;
    ngx_hash_t                     headers_set_hash;

    ngx_array_t                   *headers_source;

    ngx_array_t                   *webapp_lengths;
    ngx_array_t                   *webapp_values;

    ngx_array_t                   *redirects;
    ngx_array_t                   *cookie_domains;
    ngx_array_t                   *cookie_paths;

    ngx_str_t                      body_source;

    ngx_str_t                      method;
    ngx_str_t                      location;
    ngx_str_t                      url;

#if (NGX_HTTP_CACHE)
    ngx_http_complex_value_t       cache_key;
#endif

    ngx_http_webapp_vars_t          vars;

    ngx_flag_t                     redirect;

    ngx_uint_t                     http_version;

    ngx_uint_t                     headers_hash_max_size;
    ngx_uint_t                     headers_hash_bucket_size;

} ngx_http_webapp_loc_conf_t;


typedef struct {
    ngx_http_status_t              status;
    ngx_http_chunked_t             chunked;
    ngx_http_webapp_vars_t          vars;
    off_t                          internal_body_length;

    ngx_uint_t                     head;  /* unsigned  head:1 */
} ngx_http_webapp_ctx_t;


static ngx_int_t ngx_http_webapp_eval(ngx_http_request_t *r,
    ngx_http_webapp_ctx_t *ctx, ngx_http_webapp_loc_conf_t *plcf);
#if (NGX_HTTP_CACHE)
static ngx_int_t ngx_http_webapp_create_key(ngx_http_request_t *r);
#endif
static ngx_int_t ngx_http_webapp_create_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_webapp_reinit_request(ngx_http_request_t *r);
static void ngx_http_webapp_abort_request(ngx_http_request_t *r);
static void ngx_http_webapp_finalize_request(ngx_http_request_t *r,
    ngx_int_t rc);

static ngx_int_t ngx_http_webapp_host_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_webapp_port_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

static ngx_int_t
    ngx_http_webapp_internal_body_length_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

static ngx_int_t ngx_http_webapp_add_variables(ngx_conf_t *cf);
static void *ngx_http_webapp_create_loc_conf(ngx_conf_t *cf);

static char *ngx_http_webapp_pass(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_webapp_redirect(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_webapp_cookie_domain(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_webapp_cookie_path(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_webapp_store(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
#if (NGX_HTTP_CACHE)
static char *ngx_http_webapp_cache(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_webapp_cache_key(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
#endif


static void ngx_http_webapp_set_vars(ngx_url_t *u, ngx_http_webapp_vars_t *v);


static ngx_conf_bitmask_t  ngx_http_webapp_next_upstream_masks[] = {
    { ngx_string("error"), NGX_HTTP_UPSTREAM_FT_ERROR },
    { ngx_string("timeout"), NGX_HTTP_UPSTREAM_FT_TIMEOUT },
    { ngx_string("invalid_header"), NGX_HTTP_UPSTREAM_FT_INVALID_HEADER },
    { ngx_string("http_500"), NGX_HTTP_UPSTREAM_FT_HTTP_500 },
    { ngx_string("http_502"), NGX_HTTP_UPSTREAM_FT_HTTP_502 },
    { ngx_string("http_503"), NGX_HTTP_UPSTREAM_FT_HTTP_503 },
    { ngx_string("http_504"), NGX_HTTP_UPSTREAM_FT_HTTP_504 },
    { ngx_string("http_403"), NGX_HTTP_UPSTREAM_FT_HTTP_403 },
    { ngx_string("http_404"), NGX_HTTP_UPSTREAM_FT_HTTP_404 },
    { ngx_string("updating"), NGX_HTTP_UPSTREAM_FT_UPDATING },
    { ngx_string("off"), NGX_HTTP_UPSTREAM_FT_OFF },
    { ngx_null_string, 0 }
};


static ngx_conf_enum_t  ngx_http_webapp_http_version[] = {
    { ngx_string("1.0"), NGX_HTTP_VERSION_10 },
    { ngx_string("1.1"), NGX_HTTP_VERSION_11 },
    { ngx_null_string, 0 }
};


ngx_module_t  ngx_http_webapp_module;


static ngx_command_t  ngx_http_webapp_commands[] = {

    { ngx_string("webapp_pass"),
      NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_HTTP_LMT_CONF|NGX_CONF_TAKE1,
      ngx_http_webapp_pass,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_webapp_module_ctx = {
    ngx_http_webapp_add_variables,          /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_webapp_create_loc_conf,        /* create location configuration */
    NULL          /* merge location configuration */
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


static char  ngx_http_webapp_version[] = " HTTP/1.0" CRLF;
static char  ngx_http_webapp_version_11[] = " HTTP/1.1" CRLF;


static ngx_keyval_t  ngx_http_webapp_headers[] = {
    { ngx_string("Host"), ngx_string("$webapp_host") },
    { ngx_string("Connection"), ngx_string("close") },
    { ngx_string("Content-Length"), ngx_string("$webapp_internal_body_length") },
    { ngx_string("Transfer-Encoding"), ngx_string("") },
    { ngx_string("Keep-Alive"), ngx_string("") },
    { ngx_string("Expect"), ngx_string("") },
    { ngx_string("Upgrade"), ngx_string("") },
    { ngx_null_string, ngx_null_string }
};


static ngx_str_t  ngx_http_webapp_hide_headers[] = {
    ngx_string("Date"),
    ngx_string("Server"),
    ngx_string("X-Pad"),
    ngx_string("X-Accel-Expires"),
    ngx_string("X-Accel-Redirect"),
    ngx_string("X-Accel-Limit-Rate"),
    ngx_string("X-Accel-Buffering"),
    ngx_string("X-Accel-Charset"),
    ngx_null_string
};


#if (NGX_HTTP_CACHE)

static ngx_keyval_t  ngx_http_webapp_cache_headers[] = {
    { ngx_string("Host"), ngx_string("$webapp_host") },
    { ngx_string("Connection"), ngx_string("close") },
    { ngx_string("Content-Length"), ngx_string("$webapp_internal_body_length") },
    { ngx_string("Transfer-Encoding"), ngx_string("") },
    { ngx_string("Keep-Alive"), ngx_string("") },
    { ngx_string("Expect"), ngx_string("") },
    { ngx_string("Upgrade"), ngx_string("") },
    { ngx_string("If-Modified-Since"), ngx_string("") },
    { ngx_string("If-Unmodified-Since"), ngx_string("") },
    { ngx_string("If-None-Match"), ngx_string("") },
    { ngx_string("If-Match"), ngx_string("") },
    { ngx_string("Range"), ngx_string("") },
    { ngx_string("If-Range"), ngx_string("") },
    { ngx_null_string, ngx_null_string }
};

#endif


static ngx_http_variable_t  ngx_http_webapp_vars[] = {

    { ngx_string("webapp_host"), NULL, ngx_http_webapp_host_variable, 0,
      NGX_HTTP_VAR_CHANGEABLE|NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_NOHASH, 0 },

    { ngx_string("webapp_port"), NULL, ngx_http_webapp_port_variable, 0,
      NGX_HTTP_VAR_CHANGEABLE|NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_NOHASH, 0 },

    { ngx_string("webapp_internal_body_length"), NULL,
      ngx_http_webapp_internal_body_length_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_NOHASH, 0 },

    { ngx_null_string, NULL, NULL, 0, 0, 0 }
};


static ngx_path_init_t  ngx_http_webapp_temp_path = {
    ngx_string(NGX_HTTP_PROXY_TEMP_PATH), { 1, 2, 0 }
};


static ngx_int_t
ngx_http_webapp_handler(ngx_http_request_t *r)
{
    ngx_int_t                   rc;
    ngx_http_upstream_t        *u;
    ngx_http_webapp_ctx_t       *ctx;
    ngx_http_webapp_loc_conf_t  *plcf;

    if (ngx_http_upstream_create(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_webapp_ctx_t));
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_http_set_ctx(r, ctx, ngx_http_webapp_module);

    plcf = ngx_http_get_module_loc_conf(r, ngx_http_webapp_module);

    u = r->upstream;

    if (plcf->webapp_lengths == NULL) {
        ctx->vars = plcf->vars;

    } else {
        if (ngx_http_webapp_eval(r, ctx, plcf) != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    u->output.tag = (ngx_buf_tag_t) &ngx_http_webapp_module;

    u->conf = &plcf->upstream;

    u->create_request = ngx_http_webapp_create_request;
    u->reinit_request = ngx_http_webapp_reinit_request;
    u->abort_request = ngx_http_webapp_abort_request;
    u->finalize_request = ngx_http_webapp_finalize_request;
    r->state = 0;

    u->buffering = plcf->upstream.buffering;

    u->pipe = ngx_pcalloc(r->pool, sizeof(ngx_event_pipe_t));
    if (u->pipe == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

 
    u->pipe->input_ctx = r;

    u->input_filter_ctx = r;

    u->accel = 1;

    rc = ngx_http_read_client_request_body(r, ngx_http_upstream_init);

    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    return NGX_DONE;
}


static ngx_int_t
ngx_http_webapp_eval(ngx_http_request_t *r, ngx_http_webapp_ctx_t *ctx,
    ngx_http_webapp_loc_conf_t *plcf)
{
    u_char               *p;
    size_t                add;
    u_short               port = 5000;
    ngx_str_t             webapp;
    ngx_url_t             url;
    ngx_http_upstream_t  *u;

    if (ngx_http_script_run(r, &webapp, plcf->webapp_lengths->elts, 0,
                            plcf->webapp_values->elts)
        == NULL)
    {
        return NGX_ERROR;
    }

	u = r->upstream;

     ngx_memzero(&url, sizeof(ngx_url_t));

    url.url.len = webapp.len;
    url.url.data = webapp.data;
    url.default_port = port;
    url.uri_part = 1;
    url.no_resolve = 1;

    if (ngx_parse_url(r->pool, &url) != NGX_OK) {
        if (url.err) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "%s in upstream \"%V\"", url.err, &url.url);
        }

        return NGX_ERROR;
    }

    if (url.uri.len) {
        if (url.uri.data[0] == '?') {
            p = ngx_pnalloc(r->pool, url.uri.len + 1);
            if (p == NULL) {
                return NGX_ERROR;
            }

            *p++ = '/';
            ngx_memcpy(p, url.uri.data, url.uri.len);

            url.uri.len++;
            url.uri.data = p - 1;
        }
    }
	    ngx_http_webapp_set_vars(&url, &ctx->vars);

    u->resolved = ngx_pcalloc(r->pool, sizeof(ngx_http_upstream_resolved_t));
    if (u->resolved == NULL) {
        return NGX_ERROR;
    }

    if (url.addrs && url.addrs[0].sockaddr) {
        u->resolved->sockaddr = url.addrs[0].sockaddr;
        u->resolved->socklen = url.addrs[0].socklen;
        u->resolved->naddrs = 1;
        u->resolved->host = url.addrs[0].name;

    } else {
        u->resolved->host = url.host;
        u->resolved->port = (in_port_t) (url.no_port ? port : url.port);
        u->resolved->no_port = url.no_port;
    }

    return NGX_OK;
}

static ngx_int_t
ngx_http_webapp_create_request(ngx_http_request_t *r)
{
    size_t                        len, uri_len, loc_len, body_len;
    uintptr_t                     escape;
    ngx_buf_t                    *b;
    ngx_str_t                     method;
    ngx_uint_t                    i, unparsed_uri;
    ngx_chain_t                  *cl, *body;
    ngx_list_part_t              *part;
    ngx_table_elt_t              *header;
    ngx_http_upstream_t          *u;
    ngx_http_webapp_ctx_t         *ctx;
    ngx_http_script_code_pt       code;
    ngx_http_script_engine_t      e, le;
    ngx_http_webapp_loc_conf_t    *plcf;
    ngx_http_script_len_code_pt   lcode;

    u = r->upstream;

    plcf = ngx_http_get_module_loc_conf(r, ngx_http_webapp_module);

    b = ngx_create_temp_buf(r->pool, 40);
    if (b == NULL) {
        return NGX_ERROR;
    }

    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    cl->buf = b;




    /* add "\r\n" at the header end */
    *b->last++ = CR; *b->last++ = LF;
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http webapp header:\n\"%*s\"",
                   (size_t) (b->last - b->pos), b->pos);

    b->flush = 1;
    cl->next = NULL;

    return NGX_OK;
}


static ngx_int_t
ngx_http_webapp_reinit_request(ngx_http_request_t *r)
{
    ngx_http_webapp_ctx_t  *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_webapp_module);

    if (ctx == NULL) {
        return NGX_OK;
    }

    ctx->status.code = 0;
    ctx->status.count = 0;
    ctx->status.start = NULL;
    ctx->status.end = NULL;
    ctx->chunked.state = 0;

    r->state = 0;

    return NGX_OK;
}

static void
ngx_http_webapp_abort_request(ngx_http_request_t *r)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "abort http webapp request");

    return;
}


static void
ngx_http_webapp_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "finalize http webapp request");

    return;
}


static ngx_int_t
ngx_http_webapp_host_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_webapp_ctx_t  *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_webapp_module);

    if (ctx == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    v->len = ctx->vars.host_header.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = ctx->vars.host_header.data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_webapp_port_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_webapp_ctx_t  *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_webapp_module);

    if (ctx == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    v->len = ctx->vars.port.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = ctx->vars.port.data;

    return NGX_OK;
}



static ngx_int_t
ngx_http_webapp_internal_body_length_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_webapp_ctx_t  *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_webapp_module);

    if (ctx == NULL || ctx->internal_body_length < 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    v->data = ngx_pnalloc(r->pool, NGX_OFF_T_LEN);

    if (v->data == NULL) {
        return NGX_ERROR;
    }

    v->len = ngx_sprintf(v->data, "%O", ctx->internal_body_length) - v->data;

    return NGX_OK;
}

static ngx_int_t
ngx_http_webapp_add_variables(ngx_conf_t *cf)
{
    ngx_http_variable_t  *var, *v;

    for (v = ngx_http_webapp_vars; v->name.len; v++) {
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }

        var->get_handler = v->get_handler;
        var->data = v->data;
    }

    return NGX_OK;
}


static void *
ngx_http_webapp_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_webapp_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_webapp_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->upstream.bufs.num = 0;
     *     conf->upstream.ignore_headers = 0;
     *     conf->upstream.next_upstream = 0;
     *     conf->upstream.cache_use_stale = 0;
     *     conf->upstream.cache_methods = 0;
     *     conf->upstream.temp_path = NULL;
     *     conf->upstream.hide_headers_hash = { NULL, 0 };
     *     conf->upstream.uri = { 0, NULL };
     *     conf->upstream.location = NULL;
     *     conf->upstream.store_lengths = NULL;
     *     conf->upstream.store_values = NULL;
     *
     *     conf->method = { 0, NULL };
     *     conf->headers_source = NULL;
     *     conf->headers_set_len = NULL;
     *     conf->headers_set = NULL;
     *     conf->headers_set_hash = NULL;
     *     conf->body_set_len = NULL;
     *     conf->body_set = NULL;
     *     conf->body_source = { 0, NULL };
     *     conf->redirects = NULL;
     *     conf->ssl = 0;
     *     conf->ssl_protocols = 0;
     *     conf->ssl_ciphers = { 0, NULL };
     */

    conf->upstream.store = 0;
    conf->upstream.store_access = 660;
    conf->upstream.buffering = 1;
    conf->upstream.ignore_client_abort = 0;

    conf->upstream.local = NULL;

    conf->upstream.connect_timeout = 60000;
    conf->upstream.send_timeout = 60000;
    conf->upstream.read_timeout = 60000;

    conf->upstream.send_lowat = 0;
    conf->upstream.buffer_size = ngx_pagesize;

    conf->upstream.busy_buffers_size_conf = 2*ngx_pagesize;
    conf->upstream.max_temp_file_size_conf = 2*ngx_pagesize;
    conf->upstream.temp_file_write_size_conf = 2*ngx_pagesize;

    conf->upstream.pass_request_headers = 0;
    conf->upstream.pass_request_body = 0;

    conf->upstream.hide_headers = 0;
    conf->upstream.pass_headers = 0;

	conf->upstream.intercept_errors = 1;

    /* "webapp_cyclic_temp_file" is disabled */
    conf->upstream.cyclic_temp_file = 0;

    conf->redirect = 0;
    conf->upstream.change_buffering = 1;

    ngx_str_set(&conf->upstream.module, "webapp");



    return conf;
}



static char *
ngx_http_webapp_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_webapp_loc_conf_t *plcf = conf;

    size_t                      add;
    u_short                     port = 5000;
    ngx_str_t                  *value, *url;
    ngx_url_t                   u;
    ngx_uint_t                  n;
    ngx_http_core_loc_conf_t   *clcf;
    ngx_http_script_compile_t   sc;

    if (plcf->upstream.upstream || plcf->webapp_lengths) {
        return "is duplicate";
    }

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    clcf->handler = ngx_http_webapp_handler;

    if (clcf->name.data[clcf->name.len - 1] == '/') {
        clcf->auto_redirect = 1;
    }

    value = cf->args->elts;

    url = &value[1];

    n = ngx_http_script_variables_count(url);

    if (n) {

        ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

        sc.cf = cf;
        sc.source = url;
        sc.lengths = &plcf->webapp_lengths;
        sc.values = &plcf->webapp_values;
        sc.variables = n;
        sc.complete_lengths = 1;
        sc.complete_values = 1;

        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

        return NGX_CONF_OK;
    }

    ngx_memzero(&u, sizeof(ngx_url_t));

    u.url.len = url->len;
    u.url.data = url->data;
    u.default_port = port;
    u.uri_part = 1;
    u.no_resolve = 1;

    plcf->upstream.upstream = ngx_http_upstream_add(cf, &u, 0);
    if (plcf->upstream.upstream == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_http_webapp_set_vars(&u, &plcf->vars);

    plcf->location = clcf->name;

    if (clcf->named
        || clcf->noname)
    {
        if (plcf->vars.uri.len) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "\"webapp_pass\" cannot have URI part in "
                               "location given by regular expression, "
                               "or inside named location, "
                               "or inside \"if\" statement, "
                               "or inside \"limit_except\" block");
            return NGX_CONF_ERROR;
        }

        plcf->location.len = 0;
    }

    plcf->url = *url;

    return NGX_CONF_OK;
}

static void
ngx_http_webapp_set_vars(ngx_url_t *u, ngx_http_webapp_vars_t *v)
{
    if (u->family != AF_UNIX) {

        if (u->no_port || u->port == u->default_port) {
			
        } else {
            v->host_header.len = u->host.len + 1 + u->port_text.len;
            v->host_header.data = u->host.data;
            v->port = u->port_text;
        }

         v->port = u->port_text;

    } else {
        ngx_str_set(&v->host_header, "localhost");
        ngx_str_null(&v->port);
 
    }

    v->uri = u->uri;
}
