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
	ngx_str_t* output_chain[STRING_VARS];
	int chain_counter;
	//State machine vars.
	int read_bytes;
	int written_bytes;
	int response_size;
	u_char tmp_buf[4];
	int tmp_written;
	u_char* response_buf;
	ngx_str_t request_body;
} webapp_request_t;

static void conn_read(ngx_event_t *ev) {
	ngx_connection_t* c = ev->data;
	webapp_request_t* wr = c->data;
	ngx_http_request_t* r = wr->r;

	if(!ev->timedout && wr->chain_counter == STRING_VARS) {
		//TODO: output buffer.
		if(wr->response_size == 0) {
			int tmp_recv = ngx_recv(c, wr->tmp_buf + wr->tmp_written, 4 - wr->tmp_written);
			if(tmp_recv > 0)
				wr->tmp_written += tmp_recv;
			if(wr->response_buf == NULL && wr->tmp_written >= 4) {
				wr->response_size = ntohs(*(int*)wr->tmp_buf);
				wr->response_buf = ngx_palloc(r->pool, wr->response_size);
			}
		}
		if(wr->response_size > 0) {
			ssize_t n = ngx_recv(c, wr->response_buf + wr->read_bytes, wr->response_size - wr->read_bytes);
			if(n > 0) wr->read_bytes += n;
			if(n == -1)  {
				wr->read_bytes = wr->response_size = 51;
				wr->response_buf = (u_char*)"HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
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

	} else if(ev->timedout) {
		ngx_http_finalize_request(r, NGX_HTTP_SERVICE_UNAVAILABLE);
		ngx_close_connection(c);
		ev->complete = 1;
	}
}

static void conn_write(ngx_event_t *ev) {
	ssize_t bytes;
	ngx_str_t data_out;
	ngx_connection_t* c = ev->data;
	webapp_request_t* wr = c->data;
	ngx_http_request_t* r = wr->r;
	if(!ev->timedout) {
		if(wr->written_bytes == 0) {
			data_out.data = ngx_palloc(r->pool, PROTOCOL_VARS * sizeof(int));
			//Collect/write size information for client retrieval.
			if(r->method == NGX_HTTP_GET || r->method == NGX_HTTP_POST) {
				int content_len = 0;
				int cookie_len = 0;
				int agent_len = 0;
				int host_len = 0;
				ngx_str_t* cookies = NULL;
				ngx_str_t* host = NULL;
				if(r->method == NGX_HTTP_POST && r->headers_in.content_length != NULL)
					content_len = atoi((const char*)r->headers_in.content_length->value.data);
				if(r->headers_in.cookies.elts != NULL) {
					cookies = &(*((ngx_table_elt_t**)r->headers_in.cookies.elts))->value;
					cookie_len = cookies->len;
				}
				if(r->headers_in.host != NULL) {
					host = &r->headers_in.host->value;
					host_len = host->len;
				}
				if(r->headers_in.user_agent != NULL)
					agent_len = r->headers_in.user_agent->value.len;
				//Start building the size information.
				*(int*)(data_out.data) = htons(r->unparsed_uri.len); //URI len
				wr->output_chain[0] = &r->unparsed_uri;
				
				*(int*)(data_out.data + INT_INTERVAL(1)) = htons(host_len); //HOST len
				wr->output_chain[1] = host;

				*(int*)(data_out.data + INT_INTERVAL(2)) = htons(agent_len); //User agent length
				wr->output_chain[2] = &r->headers_in.user_agent->value;

				//TODO: Parse cookie array properly (send raw array chunks to upstream rather than unparsed headers).
				*(int*)(data_out.data + INT_INTERVAL(3)) = htons(cookie_len); //COOKIES length
				wr->output_chain[3] = cookies;

				*(int*)(data_out.data + INT_INTERVAL(4)) = htons(r->method); //HTTP Method
				
				*(int*)(data_out.data + INT_INTERVAL(5)) = htons(content_len); //Content length
				data_out.len = PROTOCOL_VARS * sizeof(int);

				if(r->method == NGX_HTTP_POST && r->request_body != NULL && r->request_body->bufs != NULL) {
					if(r->request_body->bufs->next != NULL) {
						ngx_chain_t* cl;
						ngx_buf_t* buf;
						int len = 0;
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
								current_buf_len = buf->last - buf->pos;
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

					wr->output_chain[4] = &wr->request_body;
				}

			} else {
				//Bad method type. Don't process.
				goto bad_method;
			}
		} else if(wr->chain_counter < STRING_VARS) {
			//write uri
			if(wr->output_chain[wr->chain_counter] != NULL) {
				data_out.data = wr->output_chain[wr->chain_counter]->data;
				data_out.len = wr->output_chain[wr->chain_counter]->len + 1;
			}
			else {
				data_out.len = 1;
				data_out.data = (u_char*)"\0";
			}
			wr->chain_counter++;
		} else {
			ev->complete = 1;
			return;
		}
		
		bytes = ngx_send(c, data_out.data, data_out.len);
		if(bytes < 0) {
			ngx_http_finalize_request(r, NGX_HTTP_SERVICE_UNAVAILABLE);
			ngx_close_connection(c);
			ev->complete = 1;
		} else {
			wr->written_bytes+=bytes;
		}
		return;

	} else {
		ev->complete = 1;
		ngx_http_finalize_request(r, NGX_HTTP_SERVICE_UNAVAILABLE);
		ngx_close_connection(ev->data);
	}
	return;

bad_method:
	ngx_http_finalize_request(r, NGX_HTTP_SERVICE_UNAVAILABLE);
	ngx_close_connection(c);
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
		ngx_add_timer(conn->read, 8000);
		ngx_add_timer(conn->write, 8000);

		//Make our request depend on a (sub) connection (backend peer).
		if (r->method != NGX_HTTP_POST)
			r->main->count++;
	}
}

static ngx_int_t
ngx_http_webapp_content_handler(ngx_http_request_t *r) {
	int path_level = 0;
	int rc;
	unsigned int i = 0;
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
