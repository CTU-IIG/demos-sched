# there are 3 groups of rules in this Makefile:
#  1) native build type selection - when building DEmOS natively, these allow to select
#     the build type - one of $(BUILD_TYPES) (defined below)
#  2) then, you can run `make all/clean/test` which builds DEmOS using the configured build type
#  3) ARM64 cross-compilation in release mode - `make aarch64`

# available build types for the native build
BUILD_TYPES = release release_logs debug alloc_test


all: build/build.ninja
	ninja -C $(<D) 1>&2	# Redirect everything to stderr so that QtCreator sees the error messages
# if build/build.ninja is not present, default to release build
build/build.ninja:
	$(MAKE) release

clean test: build/build.ninja
	ninja -C $(<D) $@


.PHONY: all clean test aarch64 $(BUILD_TYPES)


aarch64: MESON_OPTS=--buildtype release --cross-file meson_aarch64.txt
aarch64: build-aarch64/build.ninja
	ninja -C $(<D)
build-aarch64/build.ninja:
	meson ./build-aarch64 $(MESON_OPTS)


# configures release build
release: MESON_OPTS=--buildtype release
release: build/BUILD_TYPE_release

# release build with trace logs
release_logs: MESON_OPTS=--buildtype release -Dtrace_logs=true
release_logs: build/BUILD_TYPE_release_logs

# this is quite slow (startup time goes from 50 ms to 270 ms on my laptop)
# (most visible for tests, which take quite a bit longer to run, as they start DEmOS from scratch a lot)
debug: MESON_OPTS=--debug -Db_sanitize=address,undefined --optimization=g -Dlib_verbose=true -Dtrace_logs=true
debug: build/BUILD_TYPE_debug

# configures DEmOS to log all heap allocations after initialization finishes
# we cannot combine this with `debug`, because AddressSanitizer also overrides `new` and `delete`
alloc_test: MESON_OPTS=--debug --optimization=g -Dtrace_logs=true -Dlog_allocations=true
alloc_test: build/BUILD_TYPE_alloc_test


# configure meson for the desired build type and leave a tag file that
#  indicates the currently configured build type
build/BUILD_TYPE_%:
	rm -rf build/
	meson ./build $(MESON_OPTS)
	touch $@
