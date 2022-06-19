//
// the receiver only accepts a POST request with an Anyplay command,
// eventual data fecthing and playback is handled by a external player process
//

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <cJSON.h>
#include "anyplay.h"

#define EWS_HEADER_ONLY
#include "EmbeddableWebServer/EmbeddableWebServer.h"

static struct Server ews;
static pid_t player_pid=0;

#define SOUCE_URL_LEN 256
static char source_url[SOUCE_URL_LEN] = "";
static char playing=0;

void player_child_sig(int signum)
{
	fprintf(stderr, "- player got sig %d\n", signum);

	playing = 0;
}

static int anyplay_exec_command(const char *command, const char *param)
{
	if (strcmp(command, "source") == 0) {
		if (param != NULL) {
			strcpy(source_url, param);
			fprintf(stderr, "- set source to '%s'\n", param);
		} else
			return -1;
	}
	if (strcmp(command, "play") == 0) {
		if (strlen(source_url) == 0) {
			fprintf(stderr, "- no source set yet\n");
			return -1;
		} else {
			fprintf(stderr, "- start playing '%s'\n", source_url);
			player_pid=fork();
			if (player_pid == 0) {
				execlp("mpv", "mpv", "--quiet", source_url, NULL);
				return -1;
			} else {
				fprintf(stderr, "player_pid=%d\n", player_pid);
				playing=1;
			}
		}
	}
	if (strcmp(command, "stop") == 0) {
		fprintf(stderr, "- no source set yet\n");
		kill(player_pid, SIGHUP);
		playing = 0;
	}
	return 0;
}

int receiver_parse_json_cmd(const char *json_cmd)
{
	cJSON *status_json = cJSON_Parse(json_cmd);
        const cJSON *res = NULL;
        char id[16];
        char version[16];
        char command[16];
        char param[SOUCE_URL_LEN]; // is that enough?

        if (status_json == NULL)
                return -1;

	memset(id, 0, 16);
	memset(version, 0, 16);
	memset(command, 0, 16);
	memset(param, 0, 256);

	// first check if JSON is sane
	res = cJSON_GetObjectItemCaseSensitive(status_json, "jsonrpc");
        if (cJSON_IsString(res) && (res->valuestring != NULL)) {
                if (strncmp(res->valuestring,"2.0",3) != 0) {
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
                if (strncmp(res->valuestring,"Anyplay",7) != 0) {
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
        	strncpy(param, res->valuestring, SOUCE_URL_LEN-1);
        }

        return (anyplay_exec_command(command, param));
}


int anyplay_receiver_start (int port)
{
  signal(SIGCHLD, player_child_sig);

  serverInit(&ews);

  acceptConnectionsUntilStoppedFromEverywhereIPv4(&ews, ANYPLAY_RECEIVER_PORT);

  return 0;
}
