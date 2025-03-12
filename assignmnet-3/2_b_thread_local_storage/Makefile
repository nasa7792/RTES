INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc

CDEFS=
CFLAGS= -O3 $(INCLUDE_DIRS) $(CDEFS)
LIBS= 

HFILES= 
CFILES= 2_b_thread_indexed.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	2_b_thread_indexed

clean:
	-rm -f *.o *.d
	-rm -f perfmon 2_b_thread_indexed

distclean:
	-rm -f *.o *.d
	-rm -f 2_b_thread_indexed

pthread: pthread.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
