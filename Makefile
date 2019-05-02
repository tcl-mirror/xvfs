all: example.so

example.c: $(shell find example -type f) $(shell find lib -type f) xvfs.c.rvt xvfs-create Makefile
	./xvfs-create --directory example --name example > example.c.new
	mv example.c.new example.c

example.o: example.c xvfs-core.h Makefile
	cc -fPIC -Wall -I. -o example.o -c example.c

xvfs-core.o: xvfs-core.c xvfs-core.h Makefile
	cc -fPIC -Wall -I. -o xvfs-core.o -c xvfs-core.c

example.so: example.o xvfs-core.o Makefile
	cc -fPIC -shared -o example.so example.o xvfs-core.o

test: example.so
	echo 'load ./example.so Xvfs_example; puts OK' | tclsh | grep '^OK$$'

clean:
	rm -f example.so example.o example.c

distclean: clean

.PHONY: all clean distclean test
