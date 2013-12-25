#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
	ngx_url_t* url;
} ngx_http_webapp_loc_conf_t;
static void *ngx_http_webapp_create_loc_conf(ngx_conf_t *cf);

static char *ngx_http_webapp_pass(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static ngx_int_t ngx_http_webapp_init(ngx_conf_t *cf);


ngx_module_t  ngx_http_webapp_module;

static ngx_command_t  ngx_http_webapp_commands[] = {

    { ngx_string("webapp_pass"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_webapp_pass,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_webapp_module_ctx = {
    NULL,          /* preconfiguration */
	ngx_http_webapp_init,                                  /* postconfiguration */

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

#define PROTOCOL_VARS 6
#define STRING_VARS 5
#define INT_INTERVAL(i) sizeof(int)*i

typedef struct {
	ngx_http_request_t* r;
	//State machine vars.
	int read_bytes;
	int written_bytes;
	int response_size;
	u_char tmp_buf[2];
	int tmp_written;
	u_char* response_buf;
	ngx_str_t request_body;
} webapp_request_t;

static void webapp_request_fail(ngx_http_request_t* r, ngx_connection_t* c) {
	ngx_buf_t    *b;
	ngx_chain_t   out;

	b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
	if (b == NULL) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
			"Failed to allocate response buffer.");
	}

	b->pos = (u_char*)"HTTP/1.1 503 Service Unavailable\r\nContent-type: text/html\r\nContent-Length: 0\r\n\r\n";
	b->last = b->pos + 80;
	b->memory = 1;
	b->last_buf = 1;
	out.buf = b;
	out.next = NULL;

	ngx_http_output_filter(r, &out);
	ngx_http_finalize_request(r, NGX_HTTP_SERVICE_UNAVAILABLE);
	ngx_close_connection(c);

}

static void conn_read(ngx_event_t *ev) {
	ngx_connection_t* c = ev->data;
	webapp_request_t* wr = c->data;
	ngx_http_request_t* r = wr->r;

	if(!ev->timedout) {
		//TODO: output buffer.
		if(wr->response_size == 0) {
			int tmp_recv = ngx_recv(c, wr->tmp_buf + wr->tmp_written, 2 - wr->tmp_written);
			if(tmp_recv > 0)
				wr->tmp_written += tmp_recv;
			if(wr->response_buf == NULL && wr->tmp_written >= 2) {
				wr->response_size = ntohs(*(int*)wr->tmp_buf);
				wr->response_buf = ngx_palloc(r->pool, wr->response_size);
			}
		}
		if(wr->response_size > 0) {
			ssize_t n = ngx_recv(c, wr->response_buf + wr->read_bytes, wr->response_size - wr->read_bytes);
			if(n > 0) wr->read_bytes += n;
			if(n == -1)  {
				webapp_request_fail(r, c);
				ev->complete = 1;
				return;
			}
			//Response finished? (4 + response size received.) Backend is trusted (won't recieve any more than response_size!)
			if(wr->read_bytes == wr->response_size) {
				ngx_buf_t    *b;
				ngx_chain_t   out;

				b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
				if (b == NULL) {
					ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
						"Failed to allocate response buffer.");
				}

				b->pos = wr->response_buf; /* first position in memory of the data */
				b->last =  wr->response_buf + wr->read_bytes; /* last position */

				b->memory = 1; /* content is in read-only memory */
				/* (i.e., filters should copy it rather than rewrite in place) */

				b->last_buf = 1; /* there will be no more buffers in the request */

				out.buf = b;
				out.next = NULL;

				
				ngx_http_output_filter(r, &out);
				ngx_http_finalize_request(r, NGX_OK);

				ngx_close_connection(c);
				ev->complete = 1;
			}
		}

	} else {
		webapp_request_fail(r, c);
		ev->complete = 1;
	}
}

static void conn_write(ngx_event_t *ev) {
	ssize_t bytes;
	ngx_connection_t* c = ev->data;
	webapp_request_t* wr = c->data;
	ngx_http_request_t* r = wr->r;
	ngx_chain_t webapp_request;
	webapp_request.next = NULL;
	ngx_str_t* output_chain[STRING_VARS] = {0};
	
	size_t body_size = 0;
	if(!ev->timedout && !ev->complete) {
		if(r->method == NGX_HTTP_GET || r->method == NGX_HTTP_POST) {
			int i = 0; //For later loop.
			//Initialize possibly unused values to 0.
			size_t content_len = 0, cookie_len = 0, agent_len = 0, host_len = 0;
			
			output_chain[0] = &r->unparsed_uri;

			if(r->headers_in.cookies.elts != NULL) {
				output_chain[3] = &(*((ngx_table_elt_t**)r->headers_in.cookies.elts))->value;
				cookie_len = output_chain[3]->len;
			}
			if(r->headers_in.host != NULL) {
				output_chain[1] = &r->headers_in.host->value;
				host_len = output_chain[1]->len;
			}
			if (r->headers_in.user_agent != NULL) {
				agent_len = r->headers_in.user_agent->value.len;
				output_chain[2] = &r->headers_in.user_agent->value;
			}
			if(r->method == NGX_HTTP_POST && r->headers_in.content_length != NULL &&
				r->request_body != NULL && r->request_body->bufs != NULL) {
				content_len = atoi((const char*)r->headers_in.content_length->value.data);
			}
			
			body_size += r->unparsed_uri.len + 1;
			body_size += host_len + 1;
			body_size += agent_len + 1;
			body_size += cookie_len + 1;
			body_size += content_len + 1;
			
			
			ngx_buf_t* buf = webapp_request.buf = 
				ngx_create_temp_buf(r->pool, PROTOCOL_VARS * sizeof(int) + body_size);
			int* last = (int*)buf->pos;
			//Start building the size information.
			*last++ = htons(r->unparsed_uri.len); //URI len
			*last++ = htons(host_len); //HOST len
			*last++ = htons(agent_len); //User agent length
			//TODO: Parse cookie array properly (send raw array chunks to upstream rather than unparsed headers).
			*last++ = htons(cookie_len); //COOKIES length
			*last++ = htons(r->method); //HTTP Method
			*last++ = htons(content_len); //Content length
			//Handle HTTP Body content (only for POST requests).
			if (content_len > 0) {
				if(r->request_body->bufs->next != NULL) {
					ngx_chain_t* cl;
					ngx_buf_t* buf;
					size_t len = 0;
					for(cl = r->request_body->bufs; cl; cl = cl->next) {
						buf = cl->buf;
						len += buf->last - buf->pos;
					}
					
					if(len != 0) {
						int bytes_out = 0;
						u_char* req = ngx_palloc(r->pool, len);
						ngx_buf_t* buf;
						for(cl = r->request_body->bufs; cl; cl = cl->next) {
							int current_buf_len;
							buf = cl->buf;
							current_buf_len = (int)buf->last - (int)buf->pos;
							req = ngx_copy(req, cl->buf->pos, current_buf_len);
							bytes_out += current_buf_len;
						}
						wr->request_body.data = req - len;
						wr->request_body.len = content_len;
					}
				} else {
					ngx_buf_t* buf = r->request_body->bufs->buf;
					wr->request_body.data = buf->pos;
					wr->request_body.len = content_len;
				}
				output_chain[4] = &wr->request_body;
			} //NGX_HTTP_POST
			
			buf->last = (u_char*)last;
			//Create the header buffer.
			for(i = 0; i < STRING_VARS; i++) {
				if(output_chain[i] == NULL || output_chain[i]->data == NULL) {
					ngx_memcpy(buf->last, "\0", 1);
					buf->last++;
				} else {
					buf->last = ngx_copy(buf->last, output_chain[i]->data,
						output_chain[i]->len + 1);
				}
			}
			
			if (ngx_handle_write_event(c->write, 0) != NGX_OK) {
				ngx_http_finalize_request(r, NGX_HTTP_SERVICE_UNAVAILABLE);
				return;
			}
			
			c->send(c, buf->start, buf->end - buf->start); 

			if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
				ngx_http_finalize_request(r, NGX_HTTP_SERVICE_UNAVAILABLE);
				return;
			}
			ev->complete = 1;


			
		} //WEBAPP_METHOD_PROCESS
	} 
	return;

bad_method:
	webapp_request_fail(r, c);
	ev->complete = 1;
	return;
}

static void webapp_body_ready(ngx_http_request_t* r) {
	ngx_connection_t* conn;
	ngx_peer_connection_t* pconn;
	ngx_http_webapp_loc_conf_t* wlcf;
	webapp_request_t* webapp;

	wlcf = ngx_http_get_module_loc_conf(r, ngx_http_webapp_module); 
	webapp = ngx_palloc(r->pool, sizeof(webapp_request_t));

	pconn = ngx_pcalloc(r->pool, sizeof(ngx_peer_connection_t));

	pconn->log = r->connection->log;
	pconn->name = &wlcf->url->url;
	pconn->sockaddr = (struct sockaddr*)wlcf->url->sockaddr;
	pconn->socklen = wlcf->url->socklen;
	pconn->get = ngx_event_get_peer;
	pconn->local = NULL;
	pconn->connection = NULL;
	//Create the peer connection.
	ngx_event_connect_peer(pconn);
	conn = pconn->connection;
	if(conn != NULL) {
		//Set up the webapp structure.
		memset(webapp, 0, sizeof(webapp_request_t));
		webapp->r = r;

		//assign the request to the peer.
		conn->data = webapp;
		conn->read->handler = conn_read;
		conn->write->handler = conn_write;
		ngx_add_timer(conn->write, 3000);
		ngx_add_timer(conn->read, 3000);
		//Make our request depend on a (sub) connection (backend peer).
		if (r->method != NGX_HTTP_POST)
			r->main->count++;
	}
}

static ngx_int_t
ngx_http_webapp_content_handler(ngx_http_request_t *r) {
	int path_level = 0;
	ngx_int_t rc;
	unsigned int i = 0;
	ngx_http_webapp_loc_conf_t* wlcf;
	wlcf = ngx_http_get_module_loc_conf(r, ngx_http_webapp_module);
	if (wlcf->url == NGX_CONF_UNSET_PTR)
		return NGX_DECLINED;

	for(i = 0; i < r->uri.len; i++) {
		if(r->uri.data[i] == '/')
			path_level++;
	}

	if(r->uri.data == NULL || path_level != 1)
		return NGX_DECLINED;

	if (r->uri.len == 12 && strncmp(r->uri.data, "/favicon.ico", 12) == 0)
		return NGX_DECLINED;

	if(r->method == NGX_HTTP_POST) {
		rc = ngx_http_read_client_request_body(r, webapp_body_ready);
		 if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
			return rc;
		 }

		return NGX_DONE;
	} else {
			if(r->err_status == 0) {
				webapp_body_ready(r);
			}
			return NGX_DONE; //for now...
	}
	return NGX_OK;
}

static void *
ngx_http_webapp_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_webapp_loc_conf_t  *conf;
    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_webapp_loc_conf_t));
	conf->url = NGX_CONF_UNSET_PTR;
    return conf;
}

static ngx_int_t ngx_http_webapp_init(ngx_conf_t* cf) {
	ngx_http_core_main_conf_t  *cmcf;
	ngx_http_handler_pt        *h;
    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_webapp_content_handler;

	return NGX_OK;
}

static char *
ngx_http_webapp_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_url_t*             url;
	ngx_http_webapp_loc_conf_t * wlcf = conf;
	
	ngx_str_t* args = cf->args->elts;

	url = ngx_palloc(cf->pool, sizeof(ngx_url_t));
	ngx_memzero(url, sizeof(ngx_url_t));
	url->url = args[1];
	if (ngx_parse_url(cf->pool, url) != NGX_OK) {
         if (url->err) {
            ngx_log_error(NGX_LOG_ERR, cf->log, 0,
                          "%s in webapp \"%V\"", url->err, &url->url);
        }
		 return NULL;
	}

	wlcf->url = url;

    return NGX_CONF_OK;
}
