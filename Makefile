TCLSH_NATIVE  := tclsh
TCL_CONFIG_SH_DIR := $(shell echo 'puts [tcl::pkgconfig get libdir,runtime]' | $(TCLSH_NATIVE))
TCL_CONFIG_SH := $(TCL_CONFIG_SH_DIR)/tclConfig.sh
XVFS_ROOT_MOUNTPOINT := //xvfs:/
CPPFLAGS      := -DXVFS_ROOT_MOUNTPOINT='"$(XVFS_ROOT_MOUNTPOINT)"' -I. -DUSE_TCL_STUBS=1 -DXVFS_DEBUG $(shell . "${TCL_CONFIG_SH}" && echo "$${TCL_INCLUDE_SPEC}") $(XVFS_ADD_CPPFLAGS)
CFLAGS        := -fPIC -g3 -ggdb3 -Wall $(XVFS_ADD_CFLAGS)
LDFLAGS       := $(XVFS_ADD_LDFLAGS)
LIBS          := $(XVFS_ADD_LIBS)
TCL_LIB       := $(shell . "${TCL_CONFIG_SH}" && echo "$${TCL_LIB_SPEC}")
TCL_STUB_LIB  := $(shell . "${TCL_CONFIG_SH}" && echo "$${TCL_STUB_LIB_SPEC}")
TCLSH         := tclsh
LIB_SUFFIX    := $(shell . "${TCL_CONFIG_SH}"; echo "$${TCL_SHLIB_SUFFIX:-.so}")

all: example-standalone$(LIB_SUFFIX) example-client$(LIB_SUFFIX) example-flexible$(LIB_SUFFIX) xvfs$(LIB_SUFFIX)

example.c: $(shell find example -type f) $(shell find lib -type f) lib/xvfs/xvfs.c.rvt xvfs-create-c xvfs-create Makefile
	rm -f example.c.new.1 example.c.new.2
	./xvfs-create-c --directory example --name example > example.c.new.1
	./xvfs-create --directory example --name example > example.c.new.2
	bash -c "diff -u <(grep -v '^ *$$' example.c.new.1) <(grep -v '^ *$$' example.c.new.2)" || :
	rm -f example.c.new.2
	mv example.c.new.1 example.c

example-standalone.o: example.c xvfs-core.h xvfs-core.c Makefile
	$(CC) $(CPPFLAGS) -DXVFS_MODE_STANDALONE $(CFLAGS) -o example-standalone.o -c example.c

example-standalone$(LIB_SUFFIX): example-standalone.o Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o example-standalone$(LIB_SUFFIX) example-standalone.o $(LIBS) $(TCL_STUB_LIB)

example-client.o: example.c xvfs-core.h Makefile
	$(CC) $(CPPFLAGS) -DXVFS_MODE_CLIENT $(CFLAGS) -o example-client.o -c example.c

example-client$(LIB_SUFFIX): example-client.o Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o example-client$(LIB_SUFFIX) example-client.o $(LIBS) $(TCL_STUB_LIB)

example-flexible.o: example.c xvfs-core.h Makefile
	$(CC) $(CPPFLAGS) -DXVFS_MODE_FLEXIBLE $(CFLAGS) -o example-flexible.o -c example.c

example-flexible$(LIB_SUFFIX): example-flexible.o Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o example-flexible$(LIB_SUFFIX) example-flexible.o $(LIBS) $(TCL_STUB_LIB)

xvfs.o: xvfs-core.h xvfs-core.c Makefile
	$(CC) $(CPPFLAGS) -DXVFS_MODE_SERVER $(CFLAGS) -o xvfs.o -c xvfs-core.c

xvfs$(LIB_SUFFIX): xvfs.o Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o xvfs$(LIB_SUFFIX) xvfs.o $(LIBS) $(TCL_STUB_LIB)

# xvfs-create-standalone is a standalone (i.e., no external dependencies
# like lib/minirivet, xvfs-core.c, etc) version of "xvfs-create"
xvfs-create-standalone: $(shell find lib -type f) xvfs-create xvfs-core.c xvfs-core.h lib/xvfs/xvfs.c.rvt Makefile
	rm -f xvfs-create-standalone.new xvfs-create-standalone
	./xvfs-create --dump-tcl --remove-debug > xvfs-create-standalone.new
	chmod +x xvfs-create-standalone.new
	mv xvfs-create-standalone.new xvfs-create-standalone

xvfs-create-c: xvfs-create-c.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o xvfs-create-c xvfs-create-c.o $(LIBS)

xvfs-create-c.o: xvfs-create-c.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o xvfs-create-c.o -c xvfs-create-c.c

xvfs_random$(LIB_SUFFIX): $(shell find example -type f) $(shell find lib -type f) lib/xvfs/xvfs.c.rvt xvfs-create-random Makefile
	./xvfs-create-random | $(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -DXVFS_MODE_FLEXIBLE -x c - -shared -o xvfs_random$(LIB_SUFFIX) $(LIBS)

xvfs_synthetic$(LIB_SUFFIX): $(shell find lib -type f) lib/xvfs/xvfs.c.rvt xvfs-create-synthetic Makefile
	./xvfs-create-synthetic | $(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -DXVFS_MODE_FLEXIBLE -x c - -shared -o xvfs_synthetic$(LIB_SUFFIX) $(LIBS)

do-benchmark:
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

do-test: test

do-coverage:
	$(MAKE) clean
	$(MAKE) XVFS_ADD_CFLAGS=-coverage XVFS_ADD_LDFLAGS=-coverage
	$(MAKE) test XVFS_TEST_EXIT_ON_FAILURE=0
	rm -f xvfs-test-coverage.info
	lcov --capture --directory . --output-file xvfs-test-coverage.info
	rm -rf xvfs-test-coverage
	mkdir xvfs-test-coverage
	genhtml xvfs-test-coverage.info --output-directory xvfs-test-coverage
	rm -f xvfs-test-coverage.info

profile-bare: profile.c example.c xvfs-core.h xvfs-core.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -UUSE_TCL_STUBS -o profile-bare profile.c -ltcl

profile-gperf: profile.c example.c xvfs-core.h xvfs-core.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -pg -UUSE_TCL_STUBS -o profile-gperf profile.c -ltcl

do-profile: profile-bare profile-gperf Makefile
	rm -rf oprofile_data
	rm -f gmon.out callgrind.out
	operf ./profile-bare
	opreport
	./profile-gperf
	gprof ./profile-gperf
	valgrind --tool=callgrind --callgrind-out-file=callgrind.out ./profile-bare 10 2
	callgrind_annotate callgrind.out

do-valgrind: Makefile
	$(MAKE) test XVFS_TEST_EXIT_ON_FAILURE=0 GDB='valgrind --tool=memcheck --track-origins=yes --leak-check=full'

tclsh-local: tclsh-local.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -UUSE_TCL_STUBS -o tclsh-local tclsh-local.c $(LIBS) $(TCL_LIB)

do-asan: Makefile
	rm -f tclsh-local
	$(MAKE) tclsh-local test XVFS_TEST_EXIT_ON_FAILURE=0 CC='clang -fsanitize=address,undefined,leak' XVFS_ADD_CFLAGS='-Wno-string-plus-int' TCLSH=./tclsh-local

do-msan: Makefile
	rm -f tclsh-local
	$(MAKE) tclsh-local test XVFS_TEST_EXIT_ON_FAILURE=0 CC='clang -fsanitize=memory' XVFS_ADD_CFLAGS='-Wno-string-plus-int' TCLSH=./tclsh-local

clean:
	rm -f xvfs-create-standalone.new xvfs-create-standalone
	rm -f xvfs-create-c.o xvfs-create-c
	rm -f example.c example.c.new example.c.new.1 example.c.new.2
	rm -f example-standalone$(LIB_SUFFIX) example-standalone.o
	rm -f example-client.o example-client$(LIB_SUFFIX)
	rm -f example-flexible.o example-flexible$(LIB_SUFFIX)
	rm -f xvfs.o xvfs$(LIB_SUFFIX)
	rm -f example-standalone.gcda example-standalone.gcno
	rm -f example-client.gcda example-client.gcno
	rm -f example-flexible.gcda example-flexible.gcno
	rm -f xvfs-create-c.gcda xvfs-create-c.gcno
	rm -f xvfs_random$(LIB_SUFFIX) xvfs_synthetic$(LIB_SUFFIX)
	rm -f xvfs.gcda xvfs.gcno
	rm -f __test__.tcl
	rm -f profile-bare profile-gperf
	rm -f gmon.out
	rm -f callgrind.out
	rm -rf oprofile_data
	rm -f xvfs-test-coverage.info
	rm -rf xvfs-test-coverage
	rm -f tclsh-local

distclean: clean

.PHONY: all clean distclean test do-test do-coverage do-benchmark do-profile do-valgrind do-asan do-msan
