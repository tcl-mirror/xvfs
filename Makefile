all: example.so

example.c: $(shell find example -type f) $(shell find lib -type f) xvfs.c.rvt xvfs-create Makefile
	./xvfs-create --directory example --name example > example.c.new
	mv example.c.new example.c

example.o: example.c xvfs-core.h Makefile
	cc -fPIC -Wall -I. -o example.o -c example.c

example.so: example.o Makefile
	cc -fPIC -shared -o example.so example.o

test: example.so
	echo 'load ./example.so Xvfs_example; puts OK' | tclsh | grep '^OK$$'

clean:
	rm -f example.so example.o example.c

distclean: clean

.PHONY: all clean distclean test
