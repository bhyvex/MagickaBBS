CC=cc
CFLAGS=-I/usr/local/include
DEPS = ticproc.c

OBJ = ticproc.o ../../inih/ini.o crc32.o
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)


ticproc: $(OBJ)
	$(CC) -o ticproc -o $@ $^ $(CFLAGS) -L/usr/local/lib -lsqlite3

.PHONY: clean

clean:
	rm -f $(OBJ) ticproc
