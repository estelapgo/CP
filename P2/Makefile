CFLAGS=-g -Wall
OBJS=compress.o chunk_archive.o options.o new_queue.o comp3.o
LIBS=-lz
CC=gcc

all: comp

comp: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f *.o comp
