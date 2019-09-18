TCL_CONFIG_SH := /usr/lib64/tclConfig.sh
XVFS_ROOT_MOUNTPOINT := //xvfs:/
CPPFLAGS      := -DXVFS_ROOT_MOUNTPOINT='"$(XVFS_ROOT_MOUNTPOINT)"' -I. -DUSE_TCL_STUBS=1 -DXVFS_DEBUG $(shell . "${TCL_CONFIG_SH}" && echo "$${TCL_INCLUDE_SPEC}") $(XVFS_ADD_CPPFLAGS)
CFLAGS        := -fPIC -g3 -ggdb3 -Wall $(XVFS_ADD_CFLAGS)
LDFLAGS       := $(XVFS_ADD_LDFLAGS)
LIBS          := $(shell . "${TCL_CONFIG_SH}" && echo "$${TCL_STUB_LIB_SPEC}")
TCLSH         := tclsh
LIB_SUFFIX    := $(shell . "${TCL_CONFIG_SH}"; echo "$${TCL_SHLIB_SUFFIX:-.so}")

all: example-standalone$(LIB_SUFFIX) example-client$(LIB_SUFFIX) example-flexible$(LIB_SUFFIX) xvfs$(LIB_SUFFIX)

example.c: $(shell find example -type f) $(shell find lib -type f) xvfs.c.rvt xvfs-create Makefile
	./xvfs-create --directory example --name example > example.c.new
	mv example.c.new example.c

example-standalone.o: example.c xvfs-core.h xvfs-core.c Makefile
	$(CC) $(CPPFLAGS) -DXVFS_MODE_STANDALONE $(CFLAGS) -o example-standalone.o -c example.c

example-standalone$(LIB_SUFFIX): example-standalone.o Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o example-standalone$(LIB_SUFFIX) example-standalone.o $(LIBS)

example-client.o: example.c xvfs-core.h Makefile
	$(CC) $(CPPFLAGS) -DXVFS_MODE_CLIENT $(CFLAGS) -o example-client.o -c example.c

example-client$(LIB_SUFFIX): example-client.o Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o example-client$(LIB_SUFFIX) example-client.o $(LIBS)

example-flexible.o: example.c xvfs-core.h Makefile
	$(CC) $(CPPFLAGS) -DXVFS_MODE_FLEXIBLE $(CFLAGS) -o example-flexible.o -c example.c

example-flexible$(LIB_SUFFIX): example-flexible.o Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o example-flexible$(LIB_SUFFIX) example-flexible.o $(LIBS)

xvfs.o: xvfs-core.h xvfs-core.c Makefile
	$(CC) $(CPPFLAGS) -DXVFS_MODE_SERVER $(CFLAGS) -o xvfs.o -c xvfs-core.c

xvfs$(LIB_SUFFIX): xvfs.o Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o xvfs$(LIB_SUFFIX) xvfs.o $(LIBS)

# xvfs-create-standalone is a standalone (i.e., no external dependencies
# like lib/minirivet, xvfs-core.c, etc) version of "xvfs-create"
xvfs-create-standalone: $(shell find lib -type f) xvfs-create xvfs-core.c xvfs-core.h xvfs.c.rvt Makefile
	rm -f xvfs-create-standalone.new xvfs-create-standalone
	./xvfs-create --dump-tcl --remove-debug > xvfs-create-standalone.new
	chmod +x xvfs-create-standalone.new
	mv xvfs-create-standalone.new xvfs-create-standalone

benchmark:
	$(MAKE) clean all XVFS_ADD_CPPFLAGS="-UXVFS_DEBUG" XVFS_ADD_CFLAGS="-g0 -ggdb0 -s -O3"
	./benchmark.tcl

test: example-standalone$(LIB_SUFFIX) xvfs$(LIB_SUFFIX) example-client$(LIB_SUFFIX) example-flexible$(LIB_SUFFIX) Makefile
	rm -f __test__.tcl
	echo 'if {[catch { eval $$::env(XVFS_TEST_LOAD_COMMANDS); source $(XVFS_ROOT_MOUNTPOINT)example/main.tcl }]} { puts stderr $$::errorInfo; exit 1 }; exit 0' > __test__.tcl
	@export XVFS_ROOT_MOUNTPOINT; export XVFS_TEST_LOAD_COMMANDS; for XVFS_TEST_LOAD_COMMANDS in \
		'load ./example-standalone$(LIB_SUFFIX) Xvfs_example' \
		'load -global ./xvfs$(LIB_SUFFIX); load ./example-client$(LIB_SUFFIX) Xvfs_example' \
		'load ./xvfs$(LIB_SUFFIX); load ./example-flexible$(LIB_SUFFIX) Xvfs_example' \
		'load ./example-flexible$(LIB_SUFFIX) Xvfs_example'; do \
			echo "[$${XVFS_TEST_LOAD_COMMANDS}] $(GDB) $(TCLSH) __test__.tcl $(TCL_TEST_ARGS)"; \
			$(GDB) $(TCLSH) __test__.tcl $(TCL_TEST_ARGS) || exit 1; \
	done
	rm -f __test__.tcl

coverage:
	$(MAKE) clean
	$(MAKE) XVFS_ADD_CFLAGS=-coverage XVFS_ADD_LDFLAGS=-coverage
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
	rm -f example-standalone$(LIB_SUFFIX) example-standalone.o
	rm -f example-client.o example-client$(LIB_SUFFIX)
	rm -f example-flexible.o example-flexible$(LIB_SUFFIX)
	rm -f xvfs.o xvfs$(LIB_SUFFIX)
	rm -f example-standalone.gcda example-standalone.gcno
	rm -f example-client.gcda example-client.gcno
	rm -f example-flexible.gcda example-flexible.gcno
	rm -f xvfs.gcda xvfs.gcno
	rm -f __test__.tcl
	rm -f xvfs-test-coverage.info
	rm -rf xvfs-test-coverage

distclean: clean

.PHONY: all clean distclean test coverage benchmark
