CC=cc

DEPS = main.c

OBJ = main.o
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

JSMN = ../../deps/jsmn/libjsmn.a

all: magichat

$(JSMN):
	cd ../../deps/jsmn/ && $(MAKE)

magichat: $(OBJ) $(JSMN)
	$(CC) -o magichat -o $@ $^ $(CFLAGS) $(LDFLAGS) 

.PHONY: clean

clean:
	rm -f $(OBJ) magichat
