CC=gcc
CFLAGS=-Wall -pthread -g
LIBS=
OBJS1=sum.o options.o
OBJS2=sum2.o options.o
OBJS3=sum3.o options.o
OBJS4=sum4.o options.o
OBJS5=sum5.o options.o

PROGS= sum sum2 sum3 sum4 sum5

all: $(PROGS)

%.o : %.c
	$(CC) $(CFLAGS) -c $<

sum: $(OBJS1)
	$(CC) $(CFLAGS) -o $@ $(OBJS1) $(LIBS)

sum2: $(OBJS2)
	$(CC) $(CFLAGS) -o $@ $(OBJS2) $(LIBS)

sum3: $(OBJS3)
	$(CC) $(CFLAGS) -o $@ $(OBJS3) $(LIBS)

sum4: $(OBJS4)
	$(CC) $(CFLAGS) -o $@ $(OBJS4) $(LIBS)

sum5: $(OBJS5)
	$(CC) $(CFLAGS) -o $@ $(OBJS5) $(LIBS)

clean:
	rm -f $(PROGS) *.o *~

