INCLUDE_DIRS =
LIB_DIRS =
CC = gcc

CDEFS =
CFLAGS = -O3 $(INCLUDE_DIRS) $(CDEFS)
LIBS = -lm

HFILES =
CFILES = 2_read_write_mutex.c

SRCS = ${HFILES} ${CFILES}
OBJS = ${CFILES:.c=.o}

all: 2_read_write_mutex

2_read_write_mutex: $(OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	-rm -f *.o *.d
	-rm -f 2_read_write_mutex

distclean:
	-rm -f *.o *.d
	-rm -f 2_read_write_mutex

pthread: pthread.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
