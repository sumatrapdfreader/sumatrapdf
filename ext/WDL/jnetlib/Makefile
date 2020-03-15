# freebsd3 makefile
default: jnl.a

LDFLAGS = -pthread 
CFLAGS = -s -O2 -Wall -DTHREAD_SAFE -D_THREAD_SAFE -D_REENTRANT
CC = gcc
CPP = g++
CXX = g++

OBJS = asyncdns.o connection.o httpget.o httpserv.o listen.o util.o sercon.o

jnl.a: ${OBJS}
	-rm -f jnl.a
	ar rcs jnl.a ${OBJS}

test: ${OBJS} test.o
	$(CC) ${CFLAGS} -o test test.o ${OBJS} ${LDFLAGS}

clean:
	-rm -f ${OBJS} test jnl.a test.o
