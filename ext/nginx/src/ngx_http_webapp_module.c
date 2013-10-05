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

static unsigned char out_buf[255];
static ssize_t read_bytes = 0;
static ssize_t written_bytes = 0;

static void conn_read(ngx_event_t *ev) {
	if(!ev->timedout && written_bytes > 8) {
		ssize_t n = ngx_recv(ev->data, out_buf+read_bytes, 1);
		ngx_connection_t* c = ev->data;
		ngx_http_request_t* r = c->data;
		if(n > 0) read_bytes += n;
		if(n == -1)  {
			ngx_http_finalize_request(r, NGX_HTTP_SERVICE_UNAVAILABLE);
			ngx_close_connection(ev->data);
			read_bytes = written_bytes = 0;
		}


		if(read_bytes >= 40 && n == 0) {
			ngx_buf_t    *b;
			ngx_chain_t   out;

			b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
			if (b == NULL) {
				ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
					"Failed to allocate response buffer.");
			}

			b->pos = out_buf; /* first position in memory of the data */
			b->last = out_buf + read_bytes; /* last position */

			b->memory = 1; /* content is in read-only memory */
			/* (i.e., filters should copy it rather than rewrite in place) */

			b->last_buf = 1; /* there will be no more buffers in the request */

			r->keepalive = 0;
			
			out.buf = b;
			out.next = NULL;

			ngx_http_output_filter(r, &out);
			ngx_http_finalize_request(r, NGX_OK);
			ngx_close_connection(ev->data);
			ev->complete = 1;
			read_bytes = written_bytes = 0;
			

		}
	} else {
		ngx_connection_t* c = ev->data;
		ngx_http_request_t* r = c->data;
		ngx_http_finalize_request(r, NGX_OK);
		ngx_close_connection(ev->data);
		read_bytes = written_bytes = 0;
	}
}

static void conn_write(ngx_event_t *ev) {
	if(!ev->timedout && !ev->complete && written_bytes == 0) {
		u_char buf[] = "derp\r\n\r\n";
		written_bytes += ngx_send(ev->data, (u_char*)&buf, sizeof(buf));
	}
}

static ngx_int_t
ngx_http_webapp_content_handler(ngx_http_request_t *r) {
	if(r->err_status == 0) {
	ngx_connection_t* conn;
	ngx_peer_connection_t* pconn;
	ngx_http_webapp_loc_conf_t* wlcf = ngx_http_get_module_loc_conf(r, ngx_http_webapp_module); 

	pconn = ngx_palloc(r->pool, sizeof(ngx_peer_connection_t));
	pconn->log = r->connection->log;
	pconn->name = &wlcf->url->url;
	pconn->sockaddr = (struct sockaddr*)wlcf->url->sockaddr;
	pconn->socklen = wlcf->url->socklen;
	pconn->get = ngx_event_get_peer;
	pconn->local = NULL;

	//Create the peer connection.
	ngx_event_connect_peer(pconn);
	conn = pconn->connection;
	//assign the request to the peer.
	conn->data = r;
	conn->read->handler = conn_read;
	conn->write->handler = conn_write;
	ngx_add_timer(conn->read, 60000);
	ngx_add_timer(conn->write, 60000);

	//Make our request depend on a (sub) connection (backend peer).
	r->main->count++;
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
