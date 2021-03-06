/*
	Onion HTTP server library
	Copyright (C) 2010 David Moreno Montero

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Affero General Public License as
	published by the Free Software Foundation, either version 3 of the
	License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Affero General Public License for more details.

	You should have received a copy of the GNU Affero General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
	*/

#include <malloc.h>
#include <string.h>

#include <onion/dict.h>
#include <onion/server.h>
#include <onion/request.h>
#include <onion/types_internal.h>

#include "../ctest.h"

onion_server *server;

ssize_t empty_write(void *a, const char *b, size_t size){
	return size;
}

void setup(){
	server=onion_server_new();
	onion_server_set_write(server, (void*)empty_write);
}

void teardown(){
	onion_server_free(server);
}

void t01_create_add_free(){
	INIT_LOCAL();
	
	onion_request *req;
	int ok;
	
	req=onion_request_new(server, (void*)111, "localhost");
	FAIL_IF_NOT_EQUAL(req->socket, (void*)111);
	
	FAIL_IF_EQUAL(req,NULL);
	
#define _GET "GET / HTTP/1.1\n"
	ok=onion_request_write(req,_GET,sizeof(_GET));
	FAIL_IF_NOT(ok);
	
	onion_request_free(req);
	
	END_LOCAL();
}

void t02_create_add_free_overflow(){
	INIT_LOCAL();
	
	onion_request *req;
	int ok, i;
	
	req=onion_request_new(server, NULL, NULL);
	FAIL_IF_NOT_EQUAL(req->socket, NULL);

	FAIL_IF_EQUAL(req,NULL);
	
	char of[14096];
	for (i=0;i<sizeof(of);i++)
		of[i]='a'+i%26;
	of[i-1]='\0';
	char get[14096*4];
	
	sprintf(get,"%s %s %s",of,of,of);
	ok=onion_request_write(req,get,strlen(get));
	FAIL_IF_NOT_EQUAL(ok,OCS_INTERNAL_ERROR); 
	onion_request_clean(req);
	
	sprintf(get,"%s %s %s\n",of,of,of);
	ok=onion_request_write(req,get,strlen(get));
	FAIL_IF_NOT_EQUAL(ok,OCS_INTERNAL_ERROR); 
	onion_request_clean(req);

	
	sprintf(get,"GET %s %s\n",of,of);
	ok=onion_request_write(req,get,strlen(get));
	printf("%d\n",ok);
	FAIL_IF_NOT_EQUAL(ok,OCS_INTERNAL_ERROR); 
	
	onion_request_free(req);
	
	END_LOCAL();
}

#define FILL(a,b) onion_request_write(a,b,strlen(b))

void t03_create_add_free_full_flow(){
	INIT_LOCAL();
	
	onion_request *req;
	int ok;
	
	req=onion_request_new(server, 0,NULL);
	FAIL_IF_EQUAL(req,NULL);
	FAIL_IF_NOT_EQUAL(req->socket, 0);
	
	ok=FILL(req,"GET /myurl%20/is/very/deeply/nested?test=test&query2=query%202&more_query=%20more%20query+10&empty&empty2 HTTP/1.0\n");
	FAIL_IF_NOT(ok);
	ok=FILL(req,"Host: 127.0.0.1\r\n");
	FAIL_IF_NOT(ok);
	ok=FILL(req,"Other-Header: My header is very long and with spaces...\n");
	FAIL_IF_NOT(ok);
	ok=FILL(req,"Final-Header: This header do not get into headers as a result of now knowing if its finished, or if its multiline.\n");
	FAIL_IF_NOT(ok);
	
	FAIL_IF_EQUAL(req->flags,OR_GET|OR_HTTP11);
	
	FAIL_IF_EQUAL(req->headers, NULL);
	FAIL_IF_NOT_EQUAL_STR( onion_dict_get(req->headers,"Host"), "127.0.0.1");
	FAIL_IF_NOT_EQUAL_STR( onion_dict_get(req->headers,"Other-Header"), "My header is very long and with spaces...");

	FAIL_IF_NOT_EQUAL_STR(req->fullpath,"/myurl /is/very/deeply/nested");
  FAIL_IF_NOT_EQUAL(req->path,NULL);
  onion_request_process(req); // this should set the req->path.
	FAIL_IF_NOT_EQUAL_STR(req->path,"myurl /is/very/deeply/nested");

	FAIL_IF_EQUAL(req->GET, NULL);
	FAIL_IF_NOT_EQUAL_STR( onion_dict_get(req->GET,"test"), "test");
	FAIL_IF_NOT_EQUAL_STR( onion_dict_get(req->GET,"query2"), "query 2");
	FAIL_IF_NOT_EQUAL_STR( onion_dict_get(req->GET,"more_query"), " more query 10");
	FAIL_IF_EQUAL(onion_request_get_query(req, "empty"), NULL);
	FAIL_IF_EQUAL(onion_request_get_query(req, "empty2"), NULL);
	FAIL_IF_NOT_EQUAL(onion_request_get_query(req, "empty3"), NULL);
	
	onion_request_free(req);
	
	END_LOCAL();
}

void t04_create_add_free_GET(){
	INIT_LOCAL();
	
	onion_request *req;
	int ok;
	
	req=onion_request_new(server, 0, NULL);
	FAIL_IF_EQUAL(req,NULL);
	FAIL_IF_NOT_EQUAL(req->socket, 0);
	
	const char *query="GET /myurl%20/is/very/deeply/nested?test=test&query2=query%202&more_query=%20more%20query+10 HTTP/1.0\n"
													"Host: 127.0.0.1\n\r"
													"Other-Header: My header is very long and with spaces...\r\n\r\n";
	
	int i; // Straight write, with clean (keep alive like)
	for (i=0;i<10;i++){
		FAIL_IF_NOT_EQUAL_INT(req->flags,0);
		ok=onion_request_write(req,query,strlen(query));
		FAIL_IF_NOT_EQUAL_INT(ok, OCS_CLOSE_CONNECTION);
		FAIL_IF_EQUAL(req->flags,OR_GET|OR_HTTP11);
		
		FAIL_IF_EQUAL(req->headers, NULL);
		FAIL_IF_NOT_EQUAL_STR( onion_dict_get(req->headers,"Host"), "127.0.0.1");
		FAIL_IF_NOT_EQUAL_STR( onion_dict_get(req->headers,"Other-Header"), "My header is very long and with spaces...");
    FAIL_IF_NOT_EQUAL_STR( onion_dict_get(req->headers,"other-heaDER"), "My header is very long and with spaces...");
    FAIL_IF_NOT_EQUAL_STR( onion_request_get_header(req,"other-heaDER"), "My header is very long and with spaces...");

		FAIL_IF_NOT_EQUAL_STR(req->fullpath,"/myurl /is/very/deeply/nested");
		FAIL_IF_NOT_EQUAL_STR(req->path,"myurl /is/very/deeply/nested");

		FAIL_IF_EQUAL(req->GET,NULL);
		FAIL_IF_NOT_EQUAL_STR( onion_dict_get(req->GET,"test"), "test");
		FAIL_IF_NOT_EQUAL_STR( onion_dict_get(req->GET,"query2"), "query 2");
		FAIL_IF_NOT_EQUAL_STR( onion_dict_get(req->GET,"more_query"), " more query 10");
		
		onion_request_clean(req);
		FAIL_IF_NOT_EQUAL(req->GET,NULL);
	}
	
	onion_request_free(req);
	
	END_LOCAL();
}

void t05_create_add_free_POST(){
	INIT_LOCAL();
	
	onion_request *req;
	int ok;
	
	req=onion_request_new(server, 0,NULL);
	FAIL_IF_EQUAL(req,NULL);
	FAIL_IF_NOT_EQUAL(req->socket, 0);
	
	const char *query="POST /myurl%20/is/very/deeply/nested?test=test&query2=query%202&more_query=%20more%20query+10 HTTP/1.0\n"
													"Host: 127.0.0.1\n\rContent-Length: 50\n"
													"Other-Header: My header is very long and with spaces...\r\n\r\nempty_post=&post_data=1&post_data2=2&empty_post_2=\n";
	
	int i; // Straight write, with clean (keep alive like)
	for (i=0;i<10;i++){
		FAIL_IF_NOT_EQUAL(req->flags,0);
		ok=onion_request_write(req,query,strlen(query));
		
		FAIL_IF_NOT_EQUAL(ok, OCS_CLOSE_CONNECTION);
		FAIL_IF_EQUAL(req->flags,OR_GET|OR_HTTP11);
		
		FAIL_IF_EQUAL(req->headers, NULL);
		FAIL_IF_NOT_EQUAL_STR( onion_dict_get(req->headers,"Host"), "127.0.0.1");
		FAIL_IF_NOT_EQUAL_STR( onion_dict_get(req->headers,"Other-Header"), "My header is very long and with spaces...");

		FAIL_IF_NOT_EQUAL_STR(req->fullpath,"/myurl /is/very/deeply/nested");
		FAIL_IF_NOT_EQUAL_STR(req->path,"myurl /is/very/deeply/nested");

		FAIL_IF_EQUAL(req->GET,NULL);
		FAIL_IF_NOT_EQUAL_STR( onion_dict_get(req->GET,"test"), "test");
		FAIL_IF_NOT_EQUAL_STR( onion_dict_get(req->GET,"query2"), "query 2");
		FAIL_IF_NOT_EQUAL_STR( onion_dict_get(req->GET,"more_query"), " more query 10");

		const onion_dict *post=onion_request_get_post_dict(req);
		FAIL_IF_EQUAL(post,NULL);
		FAIL_IF_NOT_EQUAL_STR( onion_dict_get(post,"post_data"), "1");
		FAIL_IF_NOT_EQUAL_STR( onion_dict_get(post,"post_data2"), "2");
		FAIL_IF_NOT_EQUAL_STR( onion_request_get_post(req, "empty_post"), "");
		FAIL_IF_NOT_EQUAL_STR( onion_request_get_post(req, "empty_post_2"), "");

		onion_request_clean(req);
		FAIL_IF_NOT_EQUAL(req->GET,NULL);
	}
	
	onion_request_free(req);
	
	END_LOCAL();
}

void t06_create_add_free_bad_method(){
	INIT_LOCAL();
	
	onion_request *req;
	int ok;
	
	req=onion_request_new(server, (void*)111, "localhost");
	FAIL_IF_NOT_EQUAL(req->socket, (void*)111);
	
	FAIL_IF_EQUAL(req,NULL);
	
	ok=FILL(req,"XGETX / HTTP/1.1\n");
	FAIL_IF_NOT_EQUAL(ok,OCS_NOT_IMPLEMENTED);
	
	onion_request_free(req);
	
	END_LOCAL();
}

void t06_create_add_free_POST_toobig(){
	INIT_LOCAL();
	
	onion_request *req;
	int ok;
	
	req=onion_request_new(server, 0,NULL);
	FAIL_IF_EQUAL(req,NULL);
	FAIL_IF_NOT_EQUAL(req->socket, 0);
	
	onion_server_set_max_post_size(server, 10); // Very small limit
	
	const char *query="POST /myurl%20/is/very/deeply/nested?test=test&query2=query%202&more_query=%20more%20query+10 HTTP/1.0\n"
													"Host: 127.0.0.1\n\rContent-Length: 24\n"
													"Other-Header: My header is very long and with spaces...\r\n\r\npost_data=1&post_data2=2&post_data=1&post_data2=2";
	
	int i; // Straight write, with clean (keep alive like)
	for (i=0;i<10;i++){
		FAIL_IF_NOT_EQUAL(req->flags,0);
		ok=onion_request_write(req,query,strlen(query));
		
		FAIL_IF_NOT_EQUAL(ok,OCS_INTERNAL_ERROR);

		onion_request_clean(req);
		FAIL_IF_NOT_EQUAL(req->GET,NULL);
	}
	
	onion_request_free(req);
	
	END_LOCAL();
}


void t07_multiline_headers(){
	INIT_LOCAL();
	
	onion_request *req;
	int ok;
	
	onion_server_set_max_post_size(server, 1024);
	req=onion_request_new(server, 0,NULL);
	FAIL_IF_EQUAL(req,NULL);
	FAIL_IF_NOT_EQUAL(req->socket, 0);
	
	{
		const char *query="GET / HTTP/1.0\n"
											"Host: 127.0.0.1\n\rContent-Length: 24\n"
											"Other-Header: My header is very long and with several\n lines\n"
											"Extra-Other-Header: My header is very long and with several\n \n lines\n"
											"My-Other-Header: My header is very long and with several\n\tlines\n\n";
		
		ok=onion_request_write(req,query,strlen(query));
	}
	FAIL_IF_EQUAL(ok,OCS_INTERNAL_ERROR);
	FAIL_IF_NOT_EQUAL_STR(onion_request_get_header(req,"other-header"),"My header is very long and with several lines");
	FAIL_IF_NOT_EQUAL_STR(onion_request_get_header(req,"extra-other-header"),"My header is very long and with several lines");
	FAIL_IF_NOT_EQUAL_STR(onion_request_get_header(req,"My-other-header"),"My header is very long and with several lines");
	onion_request_clean(req);

	{
		const char *query="GET / HTTP/1.0\n"
											"Host: 127.0.0.1\n\rContent-Length: 24\n"
											"Other-Header: My header is very long and with several\n lines\n"
											"My-Other-Header: My header is very long and with several\nlines\n\n";
		
		ok=onion_request_write(req,query,strlen(query));
	}
	FAIL_IF_NOT_EQUAL(ok,OCS_INTERNAL_ERROR); // No \t at my-other-header
	
	onion_request_free(req);
	
	
	END_LOCAL();
}

int main(int argc, char **argv){
  START();
  
	setup();
	t01_create_add_free();
	t02_create_add_free_overflow();
	t03_create_add_free_full_flow();
	t04_create_add_free_GET();
	t05_create_add_free_POST();
	t06_create_add_free_POST_toobig();
	t07_multiline_headers();
	teardown();
	END();
}

