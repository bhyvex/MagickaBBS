OS := $(shell uname -s)

ifeq ($(OS), FreeBSD)
  MAKEFILE=Makefile.freebsd
endif
ifeq ($(OS), NetBSD)
  MAKEFILE=Makefile.netbsd
endif
ifeq ($(OS), Linux)
  MAKEFILE=Makefile.linux
endif
ifeq ($(OS), Darwin)
  MAKEFILE=Makefile.osx
endif

all: magicka

.PHONY: magicka www clean cleanwww

magicka:
	cd src && $(MAKE) -f $(MAKEFILE)

www: 
	cd src && $(MAKE) -f $(MAKEFILE).WWW

clean:
	cd src && $(MAKE) -f $(MAKEFILE) clean
	
cleanwww:
	cd src && $(MAKE) -f $(MAKEFILE).WWW clean
