
CC = gcc
CFLAGS ?= -Wall -O2 -g
LIBS = -lpthread

# cjson for JSON parsing
CFLAGS += `pkg-config --cflags libcjson`
LIBS += `pkg-config --libs libcjson`

PRG = anyplay
SRC = main.c receiver.c sender.c ews_response.c
OBJ = $(patsubst %.c,%.o,$(SRC))

all: $(PRG)

$(PRG): $(OBJ)
	$(CC) -o $(PRG) $(OBJ) $(LIBS)

clean:
	rm -f $(PRG) $(OBJ)
