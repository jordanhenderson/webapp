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
#define STRING_VARS 4
#define INT_INTERVAL(i) sizeof(int)*i
#define HEADER_HASH_COOKIE 2940209764

typedef struct {
	ngx_http_request_t* r;
	ngx_str_t* output_chain[STRING_VARS];
	int chain_counter;
	//State machine vars.
	int strings_written;
	int read_bytes;
	int written_bytes;
	int response_size;
	u_char tmp_buf[4];
	int tmp_written;
	u_char* response_buf;
} webapp_request_t;

static void conn_read(ngx_event_t *ev) {
	ngx_connection_t* c = ev->data;
	webapp_request_t* wr = c->data;
	ngx_http_request_t* r = wr->r;

	if(!ev->timedout && wr->strings_written) {
		//TODO: output buffer.
		if(wr->response_size == 0) {
			wr->tmp_written += ngx_recv(c, wr->tmp_buf + wr->tmp_written, 4 - wr->tmp_written);
			if(wr->response_buf == NULL && wr->tmp_written >= 4) {
				wr->response_size = ntohs(*(int*)wr->tmp_buf);
				wr->response_buf = ngx_palloc(r->pool, wr->response_size);
			}
		} else if(wr->response_size > 0) {
			ssize_t n = ngx_recv(c, wr->response_buf + wr->read_bytes, wr->response_size - wr->read_bytes);
			if(n > 0) wr->read_bytes += n;
			if(n == -1)  {
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

				r->keepalive = 0;

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
	ngx_http_core_main_conf_t* cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);
	if(!ev->timedout) {
		if(wr->written_bytes == 0) {
			data_out.data = ngx_palloc(r->pool, PROTOCOL_VARS * sizeof(int));
			//Collect/write size information for client retrieval.
			if(r->method == NGX_HTTP_GET || r->method == NGX_HTTP_POST) {
				int content_len = 0;
				int cookie_len = 0;
				int agent_len = 0;
				ngx_hash_elt_t* cookies = NULL;
				if(r->headers_in.content_length != NULL)
					content_len = atoi((const char*)r->headers_in.content_length->value.data) + 1;
				if(r->headers_in.cookies.elts != NULL) {
					cookies = (ngx_hash_elt_t*)ngx_hash_find(&cmcf->headers_in_hash, HEADER_HASH_COOKIE, (u_char*)"cookie", 6);
					if(cookies != NULL)
						cookie_len = cookies->len + 1;
				}
				if(r->headers_in.user_agent != NULL)
					agent_len = r->headers_in.user_agent->value.len + 1;

				//Start building the size information.
				*(int*)(data_out.data) = htons(r->uri.len + 1); //URI len
				wr->output_chain[0] = &r->uri;

				*(int*)(data_out.data + INT_INTERVAL(1)) = htons(c->addr_text.len + 1); //HOST len
				wr->output_chain[1] = &c->addr_text;

				*(int*)(data_out.data + INT_INTERVAL(2)) = htons(agent_len); //User agent length
				wr->output_chain[2] = &r->headers_in.user_agent->value;

				//TODO: Parse cookie array properly (send raw array chunks to upstream rather than unparsed headers).
				*(int*)(data_out.data + INT_INTERVAL(3)) = htons(cookie_len); //COOKIES length
				if(cookies != NULL)
					wr->output_chain[3] = (ngx_str_t*) cookies->value;

				*(int*)(data_out.data + INT_INTERVAL(4)) = htons(r->method); //HTTP Method
				
				*(int*)(data_out.data + INT_INTERVAL(5)) = htons(content_len); //Content length

				data_out.len = PROTOCOL_VARS * sizeof(int);
			} else {
				//Bad method type. Don't process.
				goto bad_method;
			}
		} else if(wr->chain_counter < STRING_VARS) {
			//write uri
			if(wr->output_chain[wr->chain_counter] != NULL)
				data_out = *wr->output_chain[wr->chain_counter];
			else {
				data_out.len = 0;
				data_out.data = NULL;
			}
			wr->chain_counter++;
		} else if(wr->chain_counter == STRING_VARS) {
			//Write the final terminator byte. (wake up the async reader on client end)
			data_out.data = (u_char*)"\0";
			data_out.len = 1;
		} else {
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

static ngx_int_t
ngx_http_webapp_content_handler(ngx_http_request_t *r) {
	if(r->err_status == 0) {
		ngx_connection_t* conn;
		ngx_peer_connection_t* pconn;
	
		ngx_http_webapp_loc_conf_t* wlcf = ngx_http_get_module_loc_conf(r, ngx_http_webapp_module); 
		webapp_request_t* webapp = ngx_palloc(r->pool, sizeof(webapp_request_t));

		pconn = ngx_palloc(r->pool, sizeof(ngx_peer_connection_t));

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
		ngx_add_timer(conn->read, 5000);
		ngx_add_timer(conn->write, 5000);

		//Make our request depend on a (sub) connection (backend peer).
		r->main->count++;
	}
	return NGX_DONE; //for now...
	} else {
		return NGX_OK;
	}
	
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
	ngx_http_core_loc_conf_t  *clcf;
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
