#include <string.h>

#include "anyplay.h"
#include "sender.h"
#include "receiver.h"

#include "EmbeddableWebServer/EmbeddableWebServer.h"

int main(int argc, char **argv)
{

    if (argc == 2) {
        if (strncmp(argv[1], "-l", 2)==0) {
            anyplay_receiver_start(ANYPLAY_RECEIVER_PORT);
        }
    }
    if (argc == 3) {
        if (argv[1][0] != '-') {
            anyplay_sender_serve_file(argv[1], argv[2]);
        }
    }
    return 0;
}
