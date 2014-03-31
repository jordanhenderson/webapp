#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <stdint.h>
#include <msgpack.h>
#include "ngx_http_webapp_module.h"

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

ngx_module_t ngx_http_webapp_module = {
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

//Upstream handling functions
static ngx_int_t ngx_http_webapp_handler(ngx_http_request_t *r) {
	ngx_http_upstream_t* u;
	ngx_http_webapp_ctx_t* ctx;
	ngx_http_webapp_loc_conf_t* wlcf;
	ngx_int_t rc;

	if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD|NGX_HTTP_POST))) {
		return NGX_HTTP_NOT_ALLOWED;
	}

	if(ngx_http_upstream_create(r) != NGX_OK)
		return NGX_HTTP_INTERNAL_SERVER_ERROR;

	u = r->upstream;

	ngx_str_set(&u->schema, "webapp://");
	u->output.tag = (ngx_buf_tag_t) &ngx_http_webapp_module;

	wlcf = ngx_http_get_module_loc_conf(r, ngx_http_webapp_module);
	u->conf = &wlcf->upstream;

	u->create_request = ngx_http_webapp_create_request;
	u->reinit_request = ngx_http_webapp_reinit_request;
	u->process_header = ngx_http_webapp_process_header;
	u->abort_request = ngx_http_webapp_abort_request;
	u->finalize_request = ngx_http_webapp_finalize_request;

	ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_webapp_ctx_t));
	if (ctx == NULL) {
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	ctx->request = r;

	ngx_http_set_ctx(r, ctx, ngx_http_webapp_module);

	u->input_filter_init = ngx_http_webapp_filter_init;
	u->input_filter = ngx_http_webapp_filter;
	u->input_filter_ctx = ctx;

	if(!(r->method & NGX_HTTP_POST)) {
        rc = ngx_http_discard_request_body(r);
		if(rc != NGX_OK) return rc;
		r->main->count++;
		ngx_http_upstream_init(r);
	} else {
		rc = ngx_http_read_client_request_body(r, ngx_http_upstream_init);
		if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
			return rc;
		}
	}
	return NGX_DONE;
}

static inline int ngx_http_webapp_msgpack_write(void* data, const char* buf, size_t len) {
    ngx_buf_t* b = (ngx_buf_t*)data;
    b->last = ngx_copy(b->last, buf, len);
}

static ngx_int_t ngx_http_webapp_create_request(ngx_http_request_t *r) {
    ngx_http_webapp_loc_conf_t* wlcf;
	ngx_chain_t* cl;
	ngx_chain_t* body;
	ngx_http_upstream_t* u;
	ngx_buf_t* b;

    ngx_str_t request_body = {0};

    msgpack_packer pk;

    int total = 0; //string length counter
    ngx_str_t* output_chain[PROTOCOL_STRINGS] = {0};
    ngx_str_t** last_p_str = output_chain;
    int chain_ctr = 0;

	u = r->upstream;
	wlcf = ngx_http_get_module_loc_conf(r, ngx_http_webapp_module);

	//Calculate request body length if provided and needed.
	if(r->method == NGX_HTTP_POST && r->headers_in.content_length != NULL &&
		r->request_body != NULL && r->request_body->bufs != NULL) {
		request_body.len
				= atoi((const char*)r->headers_in.content_length->value.data);
	}

    //STEP 3: Add header strings here.
    //Strings lengths are summed up in order to predetermine buffer size.

    ADD_PROTOCOL_(r->unparsed_uri, total)
    ADD_PROTOCOL(r->headers_in.host, r->headers_in.host->value, total)
    ADD_PROTOCOL(r->headers_in.user_agent, r->headers_in.user_agent->value, total)
    ADD_PROTOCOL(r->headers_in.cookies.elts,
                 (*((ngx_table_elt_t**)r->headers_in.cookies.elts))->value, total)

    //END


    //Create initial buffer to hold the (minimum) size of the serialized data.
    b = ngx_create_temp_buf(r->pool, MSGPACK_SIZEOF_ARRAY * 2 +
                                   MSGPACK_SIZEOF_NUMBER + //keep total size
                                   PROTOCOL_STRINGS * MSGPACK_SIZEOF_RAW +
                                   PROTOCOL_NUMS * MSGPACK_SIZEOF_NUMBER +
                                   total);
    if (b == NULL) {
        return NGX_ERROR;
    }

    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    b->last += MSGPACK_SIZEOF_NUMBER; //Reserve space for a number.

    msgpack_packer_init(&pk, b, ngx_http_webapp_msgpack_write);
    msgpack_pack_array(&pk, PROTOCOL_STRINGS + PROTOCOL_NUMS);

    //STEP 4: Add number variables here.
    msgpack_pack_uint64(&pk, request_body.len);
    msgpack_pack_int(&pk, r->method);
    //END

    //Keep a hold of any request_body content (for passing POST to upstream)
    body = u->request_bufs;

    cl->buf = b;
    //Signal Nginx to use the created chain link (top level).
    u->request_bufs = cl;

    for(chain_ctr = 0; chain_ctr < PROTOCOL_STRINGS; chain_ctr++) {
        ngx_str_t* s = output_chain[chain_ctr];
        if(s != NULL && s->data != NULL) {
            msgpack_pack_raw(&pk, s->len);
            msgpack_pack_raw_body(&pk, s->data, s->len);
        }
    }

    //Now the header is serialized. Store its size at the beginning.
    //Since we don't know how large the header size needs to be, serialize it, then copy.
    ngx_buf_t* header_length_buf = ngx_create_temp_buf(r->pool, 9);
    msgpack_packer_init(&pk, header_length_buf, ngx_http_webapp_msgpack_write);
    msgpack_pack_uint64(&pk, b->last - b->start);
    int offset = MSGPACK_SIZEOF_NUMBER - (header_length_buf->last - header_length_buf->start);
    //Copy the memory to the appropriate offset in b.
    ngx_memcpy(b->start + offset, header_length_buf->start, MSGPACK_SIZEOF_NUMBER - offset);

    //Adjust b's position to start at the beginning of the serialized number.
    b->pos += offset;

    //Finally, handle HTTP Body content.
	while (body) {
        b = ngx_alloc_buf(r->pool);
        if (b == NULL) {
            return NGX_ERROR;
        }

        ngx_memcpy(b, body->buf, sizeof(ngx_buf_t));

        cl->next = ngx_alloc_chain_link(r->pool);
        if (cl->next == NULL) {
            return NGX_ERROR;
        }

        cl = cl->next;
        cl->buf = b;

        body = body->next;
	}

	b->flush = 1;
	cl->next = NULL;

	return NGX_OK;
}

static ngx_int_t ngx_http_webapp_reinit_request(ngx_http_request_t *r) {
	return NGX_OK;
}

//Perform appropriate host order conversion on response variables.
//Return the remaining header length required to read before handling body.
static uint16_t ngx_http_webapp_read_response(ngx_http_webapp_response_t* resp) {
	resp->total_len = ntohl(resp->total_len);
	resp->content_type_len = ntohl(resp->content_type_len);
	resp->cookies_len = ntohl(resp->cookies_len);
	return resp->content_type_len + resp->cookies_len;
}

static ngx_int_t ngx_http_webapp_process_header(ngx_http_request_t *r) {
	ngx_http_upstream_t* u;
    ngx_http_headers_conf_t  *hcf;
	ngx_http_webapp_ctx_t* ctx =
			ngx_http_get_module_ctx(r, ngx_http_webapp_module);
	ngx_http_webapp_response_t* resp = &ctx->resp;
	u = r->upstream;

	//Stage 1
	if(u->buffer.pos == u->buffer.start) {
		if(u->buffer.last - u->buffer.start <
				sizeof(ngx_http_webapp_response_t)) {
			return NGX_AGAIN;
		} else {
			ngx_memcpy(resp, u->buffer.pos, 
				sizeof(ngx_http_webapp_response_t));
			ctx->remaining_header_len = 
				ngx_http_webapp_read_response(resp);
			
			u->buffer.pos = u->buffer.start + 
				sizeof(ngx_http_webapp_response_t);
		}
	}

	//Stage 2
	if(u->buffer.last - u->buffer.pos >= ctx->remaining_header_len) {
		//Remaining header read successfully.
		u->headers_in.status_n = 200;
		u->state->status = 200;
		u->headers_in.content_length_n = resp->total_len;

		if(resp->content_type_len > 0) {
			r->headers_out.content_type_len = 
				r->headers_out.content_type.len = resp->content_type_len;
			r->headers_out.content_type.data = u->buffer.pos;
			u->buffer.pos += resp->content_type_len;
		}

		//Send set-cookie header.
		if(resp->cookies_len > 0) {
			ngx_table_elt_t* set_cookie;
			set_cookie = ngx_list_push(&r->headers_out.headers);
			if (set_cookie == NULL) {
				return NGX_ERROR;
			}

			set_cookie->hash = 1;
			ngx_str_set(&set_cookie->key, "Set-Cookie");
			set_cookie->value.len = resp->cookies_len;
			set_cookie->value.data = u->buffer.pos;
			u->buffer.pos += resp->cookies_len;
		}

        //Set caching mode
        hcf = ngx_http_get_module_loc_conf(r, ngx_http_headers_filter_module);
		if(resp->cache == 0) {
            hcf->expires_time = -1;
            hcf->expires = NGX_HTTP_EXPIRES_ACCESS;
        } else {
            hcf->expires = NGX_HTTP_EXPIRES_OFF;
        }
		return NGX_OK; //Ready to read body content.
	}
	
	return NGX_AGAIN;
}

static ngx_int_t ngx_http_webapp_filter_init(void *data) {
	ngx_http_webapp_ctx_t* ctx = data;

	ngx_http_upstream_t  *u;
	u = ctx->request->upstream;

	if (u->headers_in.status_n != 404) {
		u->length = -1;
	} else {
		u->length = 0;
	}

	return NGX_OK;
}

static ngx_int_t ngx_http_webapp_filter(void *data, ssize_t bytes) {
	ngx_http_webapp_ctx_t* ctx = data;

	ngx_buf_t            *b;
	ngx_chain_t          *cl, **ll;
	ngx_http_upstream_t  *u;
	ngx_http_request_t* r = ctx->request;

	u = r->upstream;

	for (cl = u->out_bufs, ll = &u->out_bufs; cl; cl = cl->next) {
		ll = &cl->next;
	}

	cl = ngx_chain_get_free_buf(r->pool, &u->free_bufs);
	if (cl == NULL) {
		return NGX_ERROR;
	}

	*ll = cl;

	cl->buf->flush = 1;
	cl->buf->memory = 1;

	b = &u->buffer;

	cl->buf->pos = b->last;
	b->last += bytes;
	cl->buf->last = b->last;
	cl->buf->tag = u->output.tag;

	if (u->length == -1) {
		return NGX_OK;
	}

	u->length -= bytes;

	return NGX_OK;
}

static void ngx_http_webapp_abort_request(ngx_http_request_t *r) {
	ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
				   "abort http webapp request");
	return;
}

static void ngx_http_webapp_finalize_request(ngx_http_request_t *r,
												ngx_int_t rc) {
	ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
				   "finalize http webapp request");
	return;
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

	conf->upstream.buffer_size = NGX_CONF_UNSET_SIZE;

	/* the hardcoded values */
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
	conf->upstream.pass_request_body = 1;

	return conf;
}

static char* ngx_http_webapp_merge_loc_conf(ngx_conf_t *cf, void *parent,
											void *child) {
	ngx_http_webapp_loc_conf_t* prev = parent;
	ngx_http_webapp_loc_conf_t* conf = child;

	ngx_conf_merge_ptr_value(conf->upstream.local,
							  prev->upstream.local, NULL);

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
