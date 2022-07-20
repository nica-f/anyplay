
CC = gcc
CFLAGS ?= -MMD -Wall -O2 -g

# CFLAGS += -DPLAYER_MPV
# CFLAGS += -DPLAYER_MG123
CFLAGS += -DPLAYER_FFPLAY

# cjson for JSON parsing
CFLAGS += `pkg-config --cflags libcjson`
LIBS += `pkg-config --libs libcjson`

PRG = anyplay
SRC = anyplay.c webserv.c
HDR = webserv.h
OBJ = $(patsubst %.c,%.o,$(SRC))


all: $(PRG)

$(PRG): $(OBJ) $(HDR)
	$(CC) -o $(PRG) $(OBJ) $(LIBS)

-include $(SRC:.c=.d)

clean:
	rm -f $(PRG) $(OBJ) *.d
