CC=cc
CFLAGS=-I/usr/local/include
DEPS = mgpost.c
JAMLIB = ../../deps/jamlib/jamlib.a
OBJ = mgpost.o ../../src/inih/ini.o

all: mgpost

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(JAMLIB):
	cd ../../deps/jamlib && make -f Makefile.linux

mgpost: $(OBJ) $(JAMLIB)
	$(CC) -o mgpost -o $@ $^ $(CFLAGS) -L/usr/local/lib 
	
.PHONY: clean

clean:
	rm -f $(OBJ) mgpost
