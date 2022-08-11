CC = clang
CFLAGS = -Wall -Wextra -Werror -pedantic
LDFLAGS = -pthread


all: httpserver

httpserver: httpserver.o format.o datastruct.o
	$(CC) httpserver.o format.o datastruct.o -o httpserver -g $(LDFLAGS)

httpserver.o: httpserver.c format.h datastruct.h
	$(CC) $(CFLAGS) -c httpserver.c -g

format.o: format.c format.h
	$(CC) $(CFLAGS) -c format.c -g 

datastruct.o: datastruct.c datastruct.h
	$(CC) $(CFLAGS) -c datastruct.c -g

clean:
	rm *.o httpserver

format: 
	clang-format -i -style=file *.c *.h