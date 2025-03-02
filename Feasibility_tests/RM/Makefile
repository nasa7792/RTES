INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc

CDEFS=
CFLAGS= -O0 -g $(INCLUDE_DIRS) $(CDEFS)
LIBS=-lm

HFILES= 
CFILES= feasibility_tests_RM.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	feasibility_tests_RM

clean:
	-rm -f *.o *.d
	-rm -f feasibility_tests_RM

feasibility_tests_RM: feasibility_tests_RM.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o $(LIBS)  
depend:

.c.o:
	$(CC) $(CFLAGS) -c $<