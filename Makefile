CC = clang
CFLAGS = -Wall -Wextra -Werror -pedantic

.PHONY: all clean

all: httpserver

httpserver: httpserver.o List.o
	$(CC) $(CFLAGS) -o httpserver httpserver.o List.o -pthread -g

httpserver.o: httpserver.c
	$(CC) $(CFLAGS) -c httpserver.c -pthread

List.o : List.c
	$(CC) $(CFLAGS) -c List.c

clean:
	rm -f httpserver *.o

format: clean
	clang-format -i -style=file httpserver.c

valgrind:
	valgrind --leak-check=full --show-leak-kinds=all ./httpserver 8080
