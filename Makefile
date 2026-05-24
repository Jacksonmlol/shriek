CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
LDFLAGS = -pthread
LIBS = -lncursesw

all: shriek-server shriek-client

shriek-server: server.o userman.o util.o json.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

shriek-client: client.o util.o json.o commands.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

server.o: server.c userman.h util.h json.h
	$(CC) $(CFLAGS) -c -o $@ server.c

client.o: client.c util.h json.h commands.h
	$(CC) $(CFLAGS) -c -o $@ client.c

commands.o: commands.c commands.h
	$(CC) $(CFLAGS) -c -o $@ commands.c

userman.o: userman.c userman.h json.h
	$(CC) $(CFLAGS) -c -o $@ userman.c

util.o: util.c util.h
	$(CC) $(CFLAGS) -c -o $@ util.c

json.o: json.c json.h
	$(CC) $(CFLAGS) -c -o $@ json.c

clean:
	rm -f *.o shriek-server shriek-client

.PHONY: all clean
