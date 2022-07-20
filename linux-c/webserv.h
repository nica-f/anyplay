
#ifndef _WEBSERV_H
#define _WEBSERV_H

#define HomePage "<html><header><title>Anyplay</title></header><body bgcolor=\"#def28d\"><center><h3>Anyplay</h3></center></body></html>"

#define HTTP_404 "HTTP/1.1 404 Not Found\r\nServer: Anyplay\r\nContent-Type: text/html; charset=UTF-8\r\n\r\nNot Found\n"

#define HTTP_OK			"200 OK\r\n"
#define HTTP_FORBIDEN	"403 Forbidden\r\n"
#define HTTP_NOT_FOUND	"404 Not Found\r\n"

// helper functions
int find_free_port(void);
char *url_encode(char *str);
char *url_decode(char *str);

int post_jscon_req(const char *host, char *req);

// web server init on port
// if port == -1 a random non privileged port is used
// and can be queried with webserv_get_port()
int webserv_init(int port);
// accepts and handles _one_ client connection
int webserv_accept(int serverSocket);
// returns string of IP address on which server is listening
char *webserv_get_hostname(void);
// returns the listening port number
int webserv_get_port(void);
int webserv_set_media_fname(char *fname);
// 
int webserv_end(int serverSocket);

#endif

