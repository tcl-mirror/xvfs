CPPFLAGS := -I. -DUSE_TCL_STUBS=1 -DXVFS_MODE_FLEXIBLE
CFLAGS   := -fPIC -g3 -ggdb3 -Wall
LDFLAGS  :=
LIBS     := -ltclstub8.6

all: example.so

example.c: $(shell find example -type f) $(shell find lib -type f) xvfs.c.rvt xvfs-create Makefile
	./xvfs-create --directory example --name example > example.c.new
	mv example.c.new example.c

example.o: example.c xvfs-core.h Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -o example.o -c example.c

xvfs-core.o: xvfs-core.c xvfs-core.h Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -o xvfs-core.o -c xvfs-core.c

example.so: example.o xvfs-core.o Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o example.so example.o xvfs-core.o $(LIBS)

test: example.so
	echo 'if {[catch { load ./example.so Xvfs_example; source //xvfs:/example/main.tcl }]} { puts stderr $$::errorInfo; exit 1 }; exit 0' | tclsh

clean:
	rm -f example.so example.o example.c
	rm -f xvfs-core.o

distclean: clean

.PHONY: all clean distclean test
