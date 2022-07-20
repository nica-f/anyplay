// SPDX-License-Identifier: GPL-3.0
//
// Anyplay WS
//
/*
 * Copyright (C) 2022 Nicole Faerber <nicole.faerber@dpin.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <signal.h>
#include <libgen.h>

#include <cJSON.h>

#include "anyplay.h"
#include "webserv.h"

#define BACKLOG 10  // Passed to listen()


static char hostname[128]={""};
static int server_port=-1;
static bool role_sender = false;
static char media_fname[PATH_MAX];
#define SOURCE_URL_LEN 256
static char source_url[SOURCE_URL_LEN] = "";
static pid_t player_pid=0;
static bool playing=false;


int post_jscon_req(const char *host, char *req)
{
    /* first where are we going to send it? */
    int portno = ANYPLAY_RECEIVER_PORT;

    struct hostent *server;
    struct sockaddr_in serv_addr;
    int sockfd, bytes, sent, received, total;
    char message[4096];
    char response[4096];
    char tmp[128];

	/* fill in the parameters */
	sprintf(message, "POST /cmd HTTP/1.1\r\n");
	strcat(message, "Content-Type: application/json\r\n");
	sprintf(tmp, "Content-Length: %ld\r\n\r\n", (unsigned long int)strlen(req));
	strcat(message, tmp);
	strcat(message, req);

	/* What are we going to send? */
	printf("Request:\n%s\n",message);

	/* create the socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("ERROR opening socket");
		return -1;
	}

	/* lookup the ip address */
	server = gethostbyname(host);
	if (server == NULL) {
		perror("ERROR, no such host");
		return -1;
	}

	/* fill in the structure */
	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	memcpy(&serv_addr.sin_addr.s_addr,server->h_addr,server->h_length);

	/* connect the socket */
	if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
		perror("ERROR connecting");
		return -1;
	}

	/* send the request */
	total = strlen(message);
	sent = 0;
	do {
		bytes = write(sockfd,message+sent,total-sent);
		if (bytes < 0) {
			perror("ERROR writing message to socket");
			return -1;
		}
		if (bytes == 0)
			break;
		sent+=bytes;
	} while (sent < total);

	/* receive the response */
	memset(response, 0, sizeof(response));
	total = sizeof(response)-1;
	received = 0;
//	do {
		//bytes = read(sockfd, response+received, total-received);
		bytes = recv(sockfd, response, 4095, 0);
		fprintf(stderr, "resp-recv %d\n", bytes);
		if (bytes < 0) {
			perror("ERROR reading response from socket");
			return -1;
		}
//		if (bytes == 0)
//			break;
		received += bytes;
//	} while (received < total);

	/*
	 * if the number of received bytes is the total size of the
	 * array then we have run out of space to store the response
	 * and it hasn't all arrived yet - so that's a bad thing
	 */
	if (received == total)
		perror("ERROR storing complete response from socket");

	/* close the socket */
	close(sockfd);

	/* process response */
	printf("Response:\n%s\n",response);

	return 0;
}

//
// this will return the next local free unprivileged port number
//
int find_free_port(void) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        printf("socket error\n");
        return -1;
    }

    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = 0;
    if (bind(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        if (errno == EADDRINUSE) {
            fprintf(stderr, "the port is not available. already to other process\n");
            return -1;
        } else {
            fprintf(stderr, "could not bind to process (%d) %s\n", errno, strerror(errno));
            return -1;
        }
    }

    socklen_t len = sizeof(serv_addr);
    if (getsockname(sock, (struct sockaddr *)&serv_addr, &len) == -1) {
        perror("getsockname");
        return -1;
    }

    if (close (sock) < 0 ) {
        fprintf(stderr, "did not close: %s\n", strerror(errno));
    }

    return ntohs(serv_addr.sin_port);
}


/* Converts a hex character to its integer value */
static char from_hex(char ch) {
	return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/* Converts an integer value to its hex character*/
static char to_hex(char code) {
	static char hex[] = "0123456789abcdef";

	return hex[code & 15];
}

//
// Returns a url-encoded version of str
// IMPORTANT: be sure to free() the returned string after use
//
char *url_encode(char *str)
{
	char *pstr = str;
	char *buf = malloc(strlen(str) * 3 + 1);
	char *pbuf = buf;

	while (*pstr) {
		if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~') 
			*pbuf++ = *pstr;
		else if (*pstr == ' ') 
			*pbuf++ = '+';
		else 
			*pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
		pstr++;
	}
	*pbuf = '\0';

	return buf;
}

//
// Returns a url-decoded version of str
// IMPORTANT: be sure to free() the returned string after use
//
char *url_decode(char *str)
{
	char *pstr = str;
	char *buf = malloc(strlen(str) + 1);
	char *pbuf = buf;

	while (*pstr) {
	    if (*pstr == '%') {
			if (pstr[1] && pstr[2]) {
				*pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
				pstr += 2;
			}
		} else if (*pstr == '+') { 
			*pbuf++ = ' ';
		} else {
			*pbuf++ = *pstr;
		}
		pstr++;
	}
	*pbuf = '\0';

	return buf;
}

static int sendbuf(int sockfd, char *buf, size_t len)
{
    size_t pos=0;
    ssize_t ret;

    while (pos < len) {
        ret=send(sockfd, buf+pos, len-pos, 0);
        if (ret < 0)
            return ret;
        pos+=ret;
    }
    return pos;
}

static int recvbuf(int sockfd, char *buf, size_t len)
{
    ssize_t ret;

    memset(buf, 0, len);

    ret=recv(sockfd, buf, len, 0);

    return (int)ret;
}

typedef enum {
	NONE,
	TEXT_HTML, // regular web page
	APPLICATION_OCTET_STREAM, // media
	APPLICATION_JSON, // JSON response
	APPLICATION_FORM, // input form
} reply_type;

static int send_file(int sockfd, char *fname)
{
	int fd, res, ressock;
	int sent=0;
	char tbuf[1024];

	fprintf(stderr, "send_file '%s'\n", fname);
	fd = open(fname, O_RDONLY);
	if (fd < 0)
		return fd;
	do {
		res = read(fd, tbuf, sizeof(tbuf));
		if (res > 0) {
			ressock = sendbuf(sockfd, tbuf, res);
			if (ressock >= 0) {
				//fprintf(stderr, ".");
				sent += ressock;
			} else {
				close(fd);
				return ressock;
			}
		}
	} while (res > 0);
	close(fd);

	return sent;
}

// if reply is not a blob (binary) then *buf contains the text to send of length len,
// else buf contains the filename and len the length of the file to send
static int send_reply(int sockfd, char *result, bool is_file, char *buf, size_t len, reply_type type)
{
    char tbuf[4096];
    unsigned int sent=0;

	strcpy(tbuf, "HTTP/1.1 ");
	strcat(tbuf, result);
    strcat(tbuf, "Server: Anyplay\r\nCache-Control: no-cache\r\nAccept-Ranges: none\r\n");
    switch (type) {
    	case APPLICATION_OCTET_STREAM: {
	        char tstr[4096];
	        char *tbufp;

    	    strcat(tbuf, "Content-Type: application/octet-stream\r\n");
    	    tbufp=url_encode(basename(buf));
    	    sprintf(tstr, "Content-Disposition: attachment; filename=\"%s\"\r\n", tbufp);
    	    free(tbufp);
			strcat(tbuf, tstr);
    	    sprintf(tstr, "Content-Length: %ld\r\n\r\n", (unsigned long int)len);
			strcat(tbuf, tstr);
    	    sent += sendbuf(sockfd, tbuf, strlen(tbuf));
    	    if (is_file)
				sent += send_file(sockfd, buf);
			else
				fprintf(stderr, "WARN: octet-stream, !is_file\n");
	    	};
			break;
    	case TEXT_HTML: {
	        strcat(tbuf, "Content-Type: text/html; charset=UTF-8\r\n\r\n");
	        sent += sendbuf(sockfd, tbuf, strlen(tbuf));
			if ((buf != NULL)/* && (len != 0)*/) {
	    	    if (is_file)
					sent += send_file(sockfd, buf);
				else
					sent += sendbuf(sockfd, buf, strlen(buf));
			};
			};
			break;
	    case APPLICATION_JSON: {
	        char tstr[128];
    	    strcat(tbuf, "Content-Type: application/json\r\n");
    	    sprintf(tstr, "Content-Length: %ld\r\n\r\n", (unsigned long int)len);
			strcat(tbuf, tstr);
    	    sent += sendbuf(sockfd, tbuf, strlen(tbuf));
	        sent += sendbuf(sockfd, buf, strlen(buf));
		    };
			break;
		case NONE:
			break;
		default:
			break;
	}
    return sent;
}

static void player_child_sig(int signum)
{
	fprintf(stderr, "- player got sig %d\n", signum);

	playing = false;
}

static bool anyplay_exec_command(int clientSocket, const char *command, const char *param)
{
	if (strcmp(command, "source") == 0) {
		if (param != NULL) {
			strcpy(source_url, param);
			fprintf(stderr, "- set source to '%s'\n", param);
		} else
			return false;
	}

	if (strcmp(command, "play") == 0) {
		if (strlen(source_url) == 0) {
			fprintf(stderr, "- no source set yet, can't play\n");
			return false;
		} else {
			fprintf(stderr, "- start playing '%s'\n", source_url);
			player_pid=fork();
			if (player_pid == 0) {
				close(0); //stdin
				close(1); //stdout
				close(2); //stderr
				close(clientSocket); // need to close or it will remain open
#if defined(PLAYER_MPV)
				execlp("mpv", "mpv", "--quiet", source_url, NULL);
#elif defined(PLAYER_MG123)
				execlp("mpg123", "mpg123", "-q", source_url, NULL);
#elif defined(PLAYER_FFPLAY)
				execlp("ffplay", "ffplay", "-loglevel", "quiet", "-vn", "-nodisp", source_url, NULL);
#else
	#error No player defined
#endif
				// this will only get reached if there is an error from exec()
				return false;
			} else {
				fprintf(stderr, "player_pid=%d\n", player_pid);
				playing = true;
				return true;
			}
		}
	}

	if (strcmp(command, "stop") == 0) {
		if (playing && (player_pid != 0)) {
			kill(player_pid, SIGHUP);
		} else
			fprintf(stderr, "no player running\n");
		playing = false;
		player_pid = 0;
	}

	return true;
}



bool process_json_req(int clientSocket, char *json_cmd)
{
	cJSON *status_json = cJSON_Parse(json_cmd);
	const cJSON *res = NULL;
	char id[16];
	char version[16];
	char command[16];
	char param[SOURCE_URL_LEN]; // is that enough?

	if (status_json == NULL)
		return false;

	memset(id, 0, 16);
	memset(version, 0, 16);
	memset(command, 0, 16);
	memset(param, 0, 256);

	// first check if JSON is sane
	res = cJSON_GetObjectItemCaseSensitive(status_json, "jsonrpc");
	if (cJSON_IsString(res) && (res->valuestring != NULL)) {
		if (strncmp(res->valuestring,"2.0", 3) != 0) {
			return -1;
		}
	} else
		return -1;

	res = cJSON_GetObjectItemCaseSensitive(status_json, "id");
	if (cJSON_IsString(res) && (res->valuestring != NULL)) {
		strncpy(id, res->valuestring, 15);
	} else
		return -1;

	res = cJSON_GetObjectItemCaseSensitive(status_json, "service");
	if (cJSON_IsString(res) && (res->valuestring != NULL)) {
		if (strncmp(res->valuestring,"Anyplay", 7) != 0) {
			return -1;
		}
	} else
		return -1;

	res = cJSON_GetObjectItemCaseSensitive(status_json, "version");
	if (cJSON_IsString(res) && (res->valuestring != NULL)) {
		strncpy(version, res->valuestring, 15);
	} else
		return -1;

	res = cJSON_GetObjectItemCaseSensitive(status_json, "command");
	if (cJSON_IsString(res) && (res->valuestring != NULL)) {
		strncpy(command, res->valuestring, 15);
	}

	res = cJSON_GetObjectItemCaseSensitive(status_json, "param");
	if (cJSON_IsString(res) && (res->valuestring != NULL)) {
		strncpy(param, res->valuestring, SOURCE_URL_LEN-1);
	}

	return (anyplay_exec_command(clientSocket, command, param));
}

static bool process_post_req(int clientSocket, char *reqbuf)
{
	char *bp, *hp, *body;
	char headstr[80];
	int clen=0;
	reply_type rtype=NONE;

	fprintf(stderr, "POST\n");
	bp=reqbuf;
	memset(headstr, 0, sizeof(headstr));
	hp=headstr;

	// if we are in media sending role we do not process POST
	if (role_sender) {
		return false;
	}

	do {
		if ((*bp != '\r') && (*bp != '\n')) {
			*(hp++)=*bp;
		} else if (*bp=='\n') {
			// fprintf(stderr, "h: '%s'\n", headstr);
			if (strncmp(headstr, "Content-Length: ", 16) == 0) {
				clen = atoi(headstr+16);
				fprintf(stderr, "content len=%d\n", clen);
				body = reqbuf+(strlen(reqbuf)-clen);
				fprintf(stderr, "body='%s'\n", body);
			}
			if (strncmp(headstr, "Content-Type: ", 14) == 0) {
				if (strncmp(headstr+14, "application/json", 16) == 0) {
					rtype = APPLICATION_JSON;
				}
				if (strncmp(headstr+14, "application/x-www-form-urlencoded", 33) == 0) {
					rtype = APPLICATION_FORM;
				}
				fprintf(stderr, "rtype=%d\n", rtype);
			}
			memset(headstr, 0, sizeof(headstr));
			hp=headstr;
		}
	} while (*(++bp) != '\0');

	switch (rtype) {
	case APPLICATION_JSON: {
			if (process_json_req(clientSocket, body)) {
				fprintf(stderr, "JSON OK\n");
				send_reply(clientSocket, HTTP_OK, false, ANYPLAY_JSON_OK, strlen(ANYPLAY_JSON_OK), APPLICATION_JSON);
			} else {
				fprintf(stderr, "JSON FAIL\n");
				send_reply(clientSocket, HTTP_OK, false, ANYPLAY_JSON_FAIL, strlen(ANYPLAY_JSON_FAIL), APPLICATION_JSON);
			}
		}
		break;
	case APPLICATION_FORM: {
		}
		break;
	default:
		fprintf(stderr, "404\n");
		sendbuf(clientSocket, HTTP_404, strlen(HTTP_404));
		break;
	}

	return true;
}

static bool process_get_req(int clientSocket, char *reqbuf)
{
	char path[80];
	int i=0;

	memset(path, 0, 80);
	// path must not be longer than 70 char
	while ((i < strlen(reqbuf)-4) && (!isspace(reqbuf[4+i])) && (i < 79)) {
		path[i]=reqbuf[4+i];
		i++;
	}
	if (i >= 79)
		fprintf(stderr, "warn: path too long\n");

	fprintf(stderr, "GET req for '%s' ->", path);

	if (strcmp("/", path) == 0 || strcmp("/index.html", path) == 0 || strcmp("/index.htm", path) == 0) {
		if (!role_sender) {
			int fd;
			strcpy(path, "index.html");
			fd = open(path, O_RDONLY);
			if (fd >= 0) {
				close(fd);
				send_reply(clientSocket, HTTP_OK, true, path, -1, TEXT_HTML);
			} else
				send_reply(clientSocket, HTTP_OK, false, HomePage, strlen(HomePage), TEXT_HTML);
			fprintf(stderr, "200\n");
		} else {
			fprintf(stderr, "404\n");
			sendbuf(clientSocket, HTTP_404, strlen(HTTP_404));
		}
	} else if (strcmp("/media", path) == 0)  {
		struct stat bs;
		if (stat(media_fname, &bs) == 0) {
			send_reply(clientSocket, HTTP_OK, true, media_fname, bs.st_size, APPLICATION_OCTET_STREAM);
			fprintf(stderr, "200\n");
		} else {
			fprintf(stderr, "404\n");
			sendbuf(clientSocket, HTTP_404, strlen(HTTP_404));
		}
	} else {
		fprintf(stderr, "404\n");
		sendbuf(clientSocket, HTTP_404, strlen(HTTP_404));
	}

	return true;
}

int webserv_accept(int serverSocket)
{
	int clientSocket;
	char reqbuf[4096];

	clientSocket = accept(serverSocket, NULL, NULL);
	if (clientSocket < 0) {
		perror("accept");
		return -1;
	}

	memset(reqbuf, 0, sizeof(reqbuf));
	if (recvbuf(clientSocket, reqbuf, sizeof(reqbuf)) > 0) {
		// fprintf(stderr, "'%s'\n", reqbuf);
		if (strncmp(reqbuf, "GET", 3) == 0) {
			process_get_req(clientSocket, reqbuf);
		} else if (strncmp(reqbuf, "POST", 4) == 0) {
				process_post_req(clientSocket, reqbuf);
		}
	} else {
		fprintf(stderr, "recv req err\n");
	}

	close(clientSocket);

	return 0;
}

int webserv_init(int port)
{
	int serverSocket;
	struct sockaddr_in serverAddress;
	struct ifaddrs *addrs = NULL;
	struct ifaddrs *p;
	int listening;

	memset(media_fname, 0, sizeof(media_fname));

	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
	setsockopt(serverSocket, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int));

    serverAddress.sin_family = AF_INET;

	if (port < 0) {
		port = find_free_port();
		if (port < 0) {
			return -1;
		}
		role_sender = true;
	}
	serverAddress.sin_port = htons(port);
	server_port = port;

	getifaddrs(&addrs);
	p = addrs;
    while (NULL != p) {
        if (NULL != p->ifa_addr && p->ifa_addr->sa_family == AF_INET) {

            getnameinfo(p->ifa_addr, sizeof(struct sockaddr_in), hostname, sizeof(hostname), NULL, 0, NI_NUMERICHOST);
            // fprintf(stderr, "Probably listening on %s http://%s:%u\n", p->ifa_name, hostname, port);
            if (strcmp("127.0.0.1", hostname) != 0) {
				serverAddress.sin_addr.s_addr = inet_addr(hostname);
                break;
            }
        }
        p = p->ifa_next;
    }
    if (NULL != addrs) {
        freeifaddrs(addrs);
    }
    if (p == NULL) {
    	fprintf(stderr, "no interface found, using lo\n");
	    serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK); //inet_addr("127.0.0.1");
    }

    //serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);//inet_addr("127.0.0.1");
    //serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(serverSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
        perror("bind");
        return -1;
    };

    listening = listen(serverSocket, BACKLOG);
    if (listening < 0) {
        printf("Error: The server is not listening.\n");
        return -1;
    }

    // report(&serverAddress);
	signal(SIGCHLD, player_child_sig);
	signal(SIGPIPE, player_child_sig);

    return serverSocket;
}

char *webserv_get_hostname(void)
{
	return hostname;
}

int webserv_get_port(void)
{
	return server_port;
}

int webserv_set_media_fname(char *fname)
{
	int tfd;

	if (fname == NULL)
		return -1;
	if (strlen(fname) > sizeof(media_fname)-1)
		return -1;
	tfd = open(fname, O_RDONLY);
	if (tfd == -1)
		return -1;
	else
		close(tfd);

	fprintf(stderr, "fname='%s'\n", fname);
	strcpy(media_fname, fname);

	return strlen(media_fname);
}

int webserv_end(int serverSocket)
{
	close(serverSocket);

	return 0;
}

