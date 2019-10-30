SHELL = /bin/sh

CFLAGS = -g -O4 -fomit-frame-pointer -Wall -I../include

testcms.o: testcms.c

testcms: testcms.o ../src/liblcms.a
	$(CC) $(CFLAGS) testcms.o ../src/liblcms.a -o $@ -lm

all: testcms test

test: testcms
	./testcms

install:
	# Nothing to install
	
clean:
	-rm testcms.o testcms testcms.exe

	
