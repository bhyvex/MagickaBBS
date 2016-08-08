CC=cc
CFLAGS=-I/usr/local/include
DEPS = fileapprove.c

OBJ = fileapprove.o
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

fileapprove: $(OBJ)
	$(CC) -o fileapprove -o $@ $^ $(CFLAGS) -L/usr/local/lib -lsqlite3 -lncurses -lcdk

.PHONY: clean

clean:
	rm -f $(OBJ) fileapprove
