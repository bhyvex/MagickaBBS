OS := $(shell uname -s)

ifeq ($(OS), FreeBSD)
  CC=cc
  CFLAGS=-I/usr/local/include
  LDFLAGS=-L/usr/local/lib
endif
ifeq ($(OS), NetBSD)
  CC=cc
  CFLAGS=-I/usr/pkg/include
  LDFLAGS=-L/usr/pkg/lib
endif
ifeq ($(OS), Linux)
  CC=gcc
  CFLAGS=
  LDFLAGS=
endif
ifeq ($(OS), Darwin)
  CC=cc
  CFLAGS=
  LDFLAGS=
endif


CC=cc
CFLAGS=-I/usr/local/include
DEPS = fileapprove.c

OBJ = fileapprove.o
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) -I../../deps/cdk-5.0-20161210/include/

fileapprove: $(OBJ)
	$(CC) -o fileapprove -o $@ $^ ../../deps/cdk-5.0-20161210/libcdk.a $(CFLAGS) $(LDFLAGS) -lsqlite3 -lncurses

.PHONY: clean

clean:
	rm -f $(OBJ) fileapprove
