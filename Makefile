CPPFLAGS := -I. -DUSE_TCL_STUBS=1 -DXVFS_MODE_FLEXIBLE
CFLAGS   := -fPIC -g3 -ggdb3 -Wall
LDFLAGS  :=
LIBS     := -ltclstub8.6
TCLSH    := tclsh

all: example.so

example.c: $(shell find example -type f) $(shell find lib -type f) xvfs.c.rvt xvfs-create Makefile
	./xvfs-create --directory example --name example > example.c.new
	mv example.c.new example.c

example.o: example.c xvfs-core.h xvfs-core.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -o example.o -c example.c

example.so: example.o Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o example.so example.o $(LIBS)

test: example.so
	rm -f __test__.tcl
	echo 'if {[catch { load ./example.so Xvfs_example; source //xvfs:/example/main.tcl }]} { puts stderr $$::errorInfo; exit 1 }; exit 0' > __test__.tcl
	$(GDB) $(TCLSH) __test__.tcl
	rm -f __test__.tcl

clean:
	rm -f example.c example.c.new
	rm -f example.so example.o
	rm -f __test__.tcl

distclean: clean

.PHONY: all clean distclean test
