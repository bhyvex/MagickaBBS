CC=cc
CFLAGS=-I/usr/local/include
DEPS = update_pass.c

OBJ = update_pass.o
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

update_pass: $(OBJ)
	$(CC) -o update_pass -o $@ $^ $(CFLAGS) -L/usr/local/lib -lsqlite3 -lssl -lcrypto

.PHONY: clean

clean:
	rm -f $(OBJ) update_pass
