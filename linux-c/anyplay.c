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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include "anyplay.h"
#include "webserv.h"


void print_usage(void)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\tanyplay -l | <media> <receiver> | -c <cmd> <receiver>\n");
}

int main (int argc, char **argv)
{
	int skt;
	char server_url[256];
	char *fname = NULL;
	char *url = NULL;
	char *receiver = NULL;
	int port = -1;

	if (argc == 1) {
		fprintf(stderr, "too few args\n");
		return 1;
	}

//	for (int i=0; i<argc; i++)
//		fprintf(stderr, "[%d] '%s'\n", i, argv[i]);

	if (argc == 2) {
		if (strcmp(argv[1], "-l") == 0) {
			port = ANYPLAY_RECEIVER_PORT;
		} else {
			print_usage();
			return 1;
		}
	}

	if (argc == 3) {
		if (argv[1][0] == '-' || argv[2][0] == '-') {
			print_usage();
			return 1;
		} else {
			receiver = argv[2];
			if (strncmp(argv[1], "http", 4) == 0) {
				char json_req[1024];
				url = argv[1];
				snprintf(json_req, 1023, "{\"jsonrpc\": \"2.0\", \"id\": \"id\", \"service\": \"Anyplay\", \"version\": \"1.0\", \"command\": \"source\", \"param\": \"%s\" }", url);
				if (post_jscon_req(receiver, json_req) != 0)
					return 1;
				else
					return 0;
			} else
				fname = argv[1];
		}
	}

	if (argc == 4) {
		if (argv[1][0] == '-' && argv[2][0] != '-' && argv[3][0] != '-') {
			char json_req[1024];
			if (strcmp(argv[1], "-c") != 0)
				return 1;

			receiver = argv[3];
			// cmds stop, vol+, vol-
			if (strcmp(argv[2], "play") == 0) {
				snprintf(json_req, 1023, "{\"jsonrpc\": \"2.0\", \"id\": \"id\", \"service\": \"Anyplay\", \"version\": \"1.0\", \"command\": \"play\", \"param\": \"\" }");
			} else if (strcmp(argv[2], "stop") == 0) {
				snprintf(json_req, 1023, "{\"jsonrpc\": \"2.0\", \"id\": \"id\", \"service\": \"Anyplay\", \"version\": \"1.0\", \"command\": \"stop\", \"param\": \"\" }");
			} else
				return 1;
			return post_jscon_req(receiver, json_req);
		} else {
			print_usage();
			return 1;
		}
	}

	skt = webserv_init(port);

	if (fname != NULL && receiver != NULL) {
		char json_req[1024];

		if (webserv_set_media_fname(fname) == -1) {
			fprintf(stderr, "can not read from '%s'\n", fname);
			close(skt);
			return 1;
		}
		snprintf(server_url, 255, "http://%s:%u/media", webserv_get_hostname(), webserv_get_port());

		// we have a server running and now need to let the receiverb know
		snprintf(json_req, 1023, "{\"jsonrpc\": \"2.0\", \"id\": \"id\", \"service\": \"Anyplay\", \"version\": \"1.0\", \"command\": \"source\", \"param\": \"%s\" }", server_url);
		if (post_jscon_req(receiver, json_req) != 0)
			return 1;

		snprintf(json_req, 1023, "{\"jsonrpc\": \"2.0\", \"id\": \"id\", \"service\": \"Anyplay\", \"version\": \"1.0\", \"command\": \"play\", \"param\": \"\" }");
		if (post_jscon_req(receiver, json_req) != 0)
			return 1;
	} else {
		snprintf(server_url, 255, "http://%s:%u/", webserv_get_hostname(), webserv_get_port());
	}
	fprintf(stderr, "%s\n", server_url);

	if (skt > 0) {
		while (webserv_accept(skt) != -1) {
		}
	} else {
	}

	webserv_end(skt);
	return 0;
}

