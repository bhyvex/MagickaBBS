# type either "make linux" or "make win32" to compile

help:
	@echo You can use this Makefile in the following ways:
	@echo make linux ............ Make Linux binaries
	@echo make cleanlinux ....... Remove object files under Linux

freebsd:
	mkdir -p obj
	gmake -C cmnllib -f Makefile.linux
	gmake -C oslib_linux
	gmake -f Makefile.linux
		
linux :
	mkdir -p obj
	make -C cmnllib -f Makefile.linux
	make -C oslib_linux 
	make -f Makefile.linux
	
cleanfreebsd :
	gmake -C cmnllib -f Makefile.linux clean
	gmake -C oslib_linux clean
	gmake -f Makefile.linux clean

cleanlinux : 
	make -C cmnllib -f Makefile.linux clean
	make -C oslib_linux clean
	make -f Makefile.linux clean
	
