CPPFLAGS := -I. -DUSE_TCL_STUBS=1 -DXVFS_MODE_FLEXIBLE -DXVFS_DEBUG
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

# xvfs-create-standalone is a standalone (i.e., no external dependencies
# like lib/minirivet, xvfs-core.c, etc) version of "xvfs-create"
xvfs-create-standalone: $(shell find lib -type f) xvfs-create xvfs-core.c xvfs-core.h xvfs.c.rvt
	rm -f xvfs-create-standalone.new xvfs-create-standalone
	./xvfs-create --dump-tcl --remove-debug > xvfs-create-standalone.new
	chmod +x xvfs-create-standalone.new
	mv xvfs-create-standalone.new xvfs-create-standalone

test: example.so
	rm -f __test__.tcl
	echo 'if {[catch { load ./example.so Xvfs_example; source //xvfs:/example/main.tcl }]} { puts stderr $$::errorInfo; exit 1 }; exit 0' > __test__.tcl
	$(GDB) $(TCLSH) __test__.tcl $(TCL_TEST_ARGS)
	rm -f __test__.tcl

clean:
	rm -f xvfs-create-standalone.new xvfs-create-standalone
	rm -f example.c example.c.new
	rm -f example.so example.o
	rm -f __test__.tcl

distclean: clean

.PHONY: all clean distclean test
