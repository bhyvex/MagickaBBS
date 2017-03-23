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
  CFLAGS=-I/opt/local/include
  LDFLAGS=-L/opt/local/lib
endif


DEPS = magiftpd.c

OBJ = magiftpd.o ../../inih/ini.o
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)


magiftpd: $(OBJ)
	$(CC) -o magiftpd -o $@ $^ $(CFLAGS) $(LDFLAGS) -lsqlite3 -lcrypto

.PHONY: clean

clean:
	rm -f $(OBJ) magiftpd
