OS := $(shell uname -s)

ifeq ($(OS), FreeBSD)
  CC=cc
  CFLAGS=-I/usr/local/include
  LDFLAGS=-L/usr/local/lib
endif
ifeq ($(OS), DragonFly)
  CC=cc
  CFLAGS=-I/usr/local/include
  LDFLAGS=-L/usr/local/lib
endif
ifeq ($(OS), NetBSD)
  CC=cc
  CFLAGS=-I/usr/pkg/include
  LDFLAGS=-R/usr/pkg/lib -L/usr/pkg/lib
endif
ifeq ($(OS), OpenBSD)
  CC=cc
  CFLAGS=-I/usr/local/include
  LDFLAGS=-L/usr/local/lib
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


DEPS = reset_pass.c

OBJ = reset_pass.o
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)


reset_pass: $(OBJ)
	$(CC) -o reset_pass $^ $(CFLAGS) $(LDFLAGS) -lsqlite3 -lssl -lcrypto

.PHONY: clean

clean:
	rm -f $(OBJ) reset_pass
