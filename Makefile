CPPFLAGS := -I. -DUSE_TCL_STUBS=1 -DXVFS_DEBUG $(XVFS_ADD_CPPFLAGS)
CFLAGS   := -fPIC -g3 -ggdb3 -Wall $(XVFS_ADD_CFLAGS)
LDFLAGS  := $(XVFS_ADD_LDFLAGS)
LIBS     := -ltclstub8.6
TCLSH    := tclsh

all: example.so example-client-server.so

example.c: $(shell find example -type f) $(shell find lib -type f) xvfs.c.rvt xvfs-create Makefile
	./xvfs-create --directory example --name example > example.c.new
	mv example.c.new example.c

example.o: example.c xvfs-core.h xvfs-core.c Makefile
	$(CC) $(CPPFLAGS) -DXVFS_MODE_FLEXIBLE $(CFLAGS) -o example.o -c example.c

example.so: example.o Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o example.so example.o $(LIBS)

example-client.o: example.c xvfs-core.h Makefile
	$(CC) $(CPPFLAGS) -DXVFS_MODE_CLIENT $(CFLAGS) -o example-client.o -c example.c

example-server.o: xvfs-core.h xvfs-core.c Makefile
	$(CC) $(CPPFLAGS) -DXVFS_MODE_SERVER $(CFLAGS) -o example-server.o -c xvfs-core.c

example-client-server.so: example-client.o example-server.o Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o example-client-server.so example-client.o example-server.o $(LIBS)

# xvfs-create-standalone is a standalone (i.e., no external dependencies
# like lib/minirivet, xvfs-core.c, etc) version of "xvfs-create"
xvfs-create-standalone: $(shell find lib -type f) xvfs-create xvfs-core.c xvfs-core.h xvfs.c.rvt Makefile
	rm -f xvfs-create-standalone.new xvfs-create-standalone
	./xvfs-create --dump-tcl --remove-debug > xvfs-create-standalone.new
	chmod +x xvfs-create-standalone.new
	mv xvfs-create-standalone.new xvfs-create-standalone

test: example.so example-client-server.so Makefile
	rm -f __test__.tcl
	echo 'if {[catch { load $$::env(XVFS_SHARED_OBJECT) Xvfs_example; source //xvfs:/example/main.tcl }]} { puts stderr $$::errorInfo; exit 1 }; exit 0' > __test__.tcl
	XVFS_SHARED_OBJECT=./example.so $(GDB) $(TCLSH) __test__.tcl $(TCL_TEST_ARGS)
	XVFS_SHARED_OBJECT=./example-client-server.so $(GDB) $(TCLSH) __test__.tcl $(TCL_TEST_ARGS)
	rm -f __test__.tcl

coverage:
	$(MAKE) clean
	$(MAKE) example.so XVFS_ADD_CFLAGS=-coverage XVFS_ADD_LDFLAGS=-coverage
	$(MAKE) test XVFS_TEST_EXIT_ON_FAILURE=0
	rm -f xvfs-test-coverage.info
	lcov --capture --directory . --output-file xvfs-test-coverage.info
	rm -rf xvfs-test-coverage
	mkdir xvfs-test-coverage
	genhtml xvfs-test-coverage.info --output-directory xvfs-test-coverage
	rm -f xvfs-test-coverage.info

clean:
	rm -f xvfs-create-standalone.new xvfs-create-standalone
	rm -f example.c example.c.new
	rm -f example.so example.o
	rm -f example-client.o example-server.o example-client-server.so
	rm -f example.gcda example.gcno
	rm -f __test__.tcl
	rm -f xvfs-test-coverage.info
	rm -rf xvfs-test-coverage

distclean: clean

.PHONY: all clean distclean test coverage
