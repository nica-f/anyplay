#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <cJSON.h>
#include "anyplay.h"
#include "receiver.h"
#include "sender.h"

#define EWS_HEADER_ONLY
#include "EmbeddableWebServer/EmbeddableWebServer.h"


struct Response* createResponseForRequest(const struct Request* request, struct Connection* connection)
{
        if (0 == strcmp(request->pathDecoded, "/")) {
                return responseAllocHTML("<html><body><marquee><h1>Welcome to my home page</h1></marquee></body></html>");
        }

        if (0 == strcmp(request->pathDecoded, "/cmd")) {
                fprintf(stderr, "%s\n", request->body.contents);

                if (receiver_parse_json_cmd(request->body.contents) == 0) {
                        return responseAllocWithFormat(200, "OK", "application/json", "{ \"status\" : \"OK\" }");
                } else {
                        return responseAllocWithFormat(200, "notOK", "application/json", "{ \"status\" : \"FAIL\" }");
                }
        }

        if (0 == strcmp(request->pathDecoded, "/100_random_numbers")) {
                struct Response* response = responseAllocHTML("<html><body><h1>100 Random Numbers</h1><ol>");
                for (int i = 1; i <= 100; i++) {
                        heapStringAppendFormat(&response->body, "<li>%d</li>\n", rand());
                }
                heapStringAppendString(&response->body, "</ol></body></html>");
                return response;
        }

        /* Serve files from the current directory */
        if (request->pathDecoded == strstr(request->pathDecoded, "/media")) {
                //fprintf(stderr, "request->path = '%s'\n", request->path);
                //fprintf(stderr, "request->pathDecoded = '%s'\n", request->pathDecoded);
                // return responseAllocServeFileFromRequestPath("/files", request->path, request->pathDecoded, ".");
                if (serve_filename != NULL) {
                        // return responseAllocServeFileFromRequestPath("/media", request->path, request->pathDecoded, serve_filename);
                        return responseAllocWithFile(serve_filename, NULL);
                }
        }

        return responseAlloc404NotFoundHTML("What?!");
}

