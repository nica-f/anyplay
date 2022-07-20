#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h> /* exit, atoi, malloc, free */
#include <netdb.h> /* struct hostent, gethostbyname */

#define EWS_HEADER_ONLY
#include "EmbeddableWebServer/EmbeddableWebServer.h"

#include "anyplay.h"
#include "sender.h"

static struct Server ews;
char *serve_filename=NULL;

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
            printf("the port is not available. already to other process\n");
            return -1;
        } else {
            printf("could not bind to process (%d) %s\n", errno, strerror(errno));
            return -1;
        }
    }

    socklen_t len = sizeof(serv_addr);
    if (getsockname(sock, (struct sockaddr *)&serv_addr, &len) == -1) {
        perror("getsockname");
        return -1;
    }

    if (close (sock) < 0 ) {
        printf("did not close: %s\n", strerror(errno));
    }

    return ntohs(serv_addr.sin_port);
}

static THREAD_RETURN_TYPE STDCALL_ON_WIN32 acceptConnectionsThread(void *Param) {
    const uint16_t portInHostOrder = *(uint16_t *)Param;

    // serverInit(&ews);
    acceptConnectionsUntilStoppedFromEverywhereIPv4(&ews, portInHostOrder);

    return (THREAD_RETURN_TYPE) 0;
}

void error(const char *msg)
{
    perror(msg);
}


int post_req(const char *host, char *req)
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
    {
        sprintf(message, "POST /cmd HTTP/1.1\r\n");
        strcat(message, "Content-Type: application/json\r\n");
        sprintf(tmp, "Content-Length: %ld\r\n\r\n", strlen(req));
        strcat(message, tmp);
        strcat(message, req);
    }

    /* What are we going to send? */
    printf("Request:\n%s\n",message);

    /* create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* lookup the ip address */
    server = gethostbyname(host);
    if (server == NULL)
        error("ERROR, no such host");

    /* fill in the structure */
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    memcpy(&serv_addr.sin_addr.s_addr,server->h_addr,server->h_length);

    /* connect the socket */
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    /* send the request */
    total = strlen(message);
    sent = 0;
    do {
        bytes = write(sockfd,message+sent,total-sent);
        if (bytes < 0)
            error("ERROR writing message to socket");
        if (bytes == 0)
            break;
        sent+=bytes;
    } while (sent < total);

    /* receive the response */
    memset(response, 0, sizeof(response));
    total = sizeof(response)-1;
    received = 0;
    do {
        //bytes = read(sockfd, response+received, total-received);
        bytes = recv(sockfd, response, 4095, 0);
        fprintf(stderr, "resp-recv %d\n", bytes);
        if (bytes < 0)
            error("ERROR reading response from socket");
        if (bytes == 0)
            break;
        received += bytes;
    } while (received < total);

    /*
     * if the number of received bytes is the total size of the
     * array then we have run out of space to store the response
     * and it hasn't all arrived yet - so that's a bad thing
     */
    if (received == total)
        error("ERROR storing complete response from socket");

    /* close the socket */
    close(sockfd);

    /* process response */
    printf("Response:\n%s\n",response);

    return 0;
}



int anyplay_sender_serve_file(char *fname, const char *receiver_ip)
{
    int port;
    int fd;
    pthread_t threadHandle;
    struct ifaddrs* addrs = NULL;
    char serve_url[256];
    char json_req[1024];

    if (fname == NULL) {
        fprintf(stderr, "filename==NULL\n");
        return -1;
    }
    fd=open(fname, O_RDONLY);
    if (fd>0) {
        close(fd);
        serve_filename = fname;
    } else {
        fprintf(stderr, "file '%s' not readable\n", fname);
        perror(NULL);
        return -1;
    }
    port = find_free_port();
    // fprintf(stderr, "free port for serving: %d\n", port);

    serverInit(&ews);
    pthread_create(&threadHandle, NULL, &acceptConnectionsThread, &port);

    getifaddrs(&addrs);
    struct ifaddrs* p = addrs;
    while (NULL != p) {
        if (NULL != p->ifa_addr && p->ifa_addr->sa_family == AF_INET) {
            char hostname[128];
            getnameinfo(p->ifa_addr, sizeof(struct sockaddr_in), hostname, sizeof(hostname), NULL, 0, NI_NUMERICHOST);
            // fprintf(stderr, "Probably listening on %s http://%s:%u\n", p->ifa_name, hostname, port);
            if (strcmp("lo", p->ifa_name) != 0) {
                snprintf(serve_url, 255, "http://%s:%u/media", hostname, port);
                fprintf(stderr, "serving on: '%s'\n", serve_url);
                break;
            }
        }
        p = p->ifa_next;
    }
    if (NULL != addrs) {
        freeifaddrs(addrs);
    }

    // we have a server running and now need to let the receiverb know
    snprintf(json_req, 1023, "{\"jsonrpc\": \"2.0\", \"id\": \"id\", \"service\": \"Anyplay\", \"version\": \"1.0\", \"command\": \"source\", \"param\": \"%s\" }", serve_url);
    post_req(receiver_ip, json_req);

    snprintf(json_req, 1023, "{\"jsonrpc\": \"2.0\", \"id\": \"id\", \"service\": \"Anyplay\", \"version\": \"1.0\", \"command\": \"play\", \"param\": \"\" }");
    post_req(receiver_ip, json_req);

    getchar();

    //serverStop(&ews);

    return 0;
}
