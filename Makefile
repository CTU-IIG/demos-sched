# FIXME: currently, if meson was not configured yet, `all` and `clean` targets don't
#  invoke `release`, but build/build.ninja directly, and BUILD_TYPE_release is not created

# this actually builds DEmOS, defaulting to release build if not previously configured otherwise
all: build/build.ninja
	ninja -C $(<D) 1>&2	# Redirect everything to stderr so that QtCreator sees the error messages

.PHONY: all clean test aarch64   release debug alloc_test

clean test: build/build.ninja
	ninja -C $(<D) $@

aarch64: export MESON_OPTS=--cross-file aarch64.txt
aarch64: build-aarch64/build.ninja
	ninja -C $(<D)

# configures release build
release: export MESON_OPTS=
release: build/BUILD_TYPE_release

# this is quite slow (startup time goes from 50 ms to 270 ms on my laptop)
# (most visible for tests, which take quite a bit longer to run, as they start DEmOS from scratch a lot)
debug: export MESON_OPTS=--debug -Db_sanitize=address,undefined --optimization=g -Dlib_verbose=true -Dtrace_logs=true
debug: build/BUILD_TYPE_debug

# configures DEmOS to log all heap allocations after initialization finishes
# we cannot combine this with `debug`, because AddressSanitizer also overrides `new` and `delete`
alloc_test: export MESON_OPTS=--debug --optimization=g -Dtrace_logs=true -Dlog_allocations=true
alloc_test: build/BUILD_TYPE_alloc_test


# this target is used so that e.g. `make debug` actually reconfigures the build
#  when it's currently a different type; if build/build.ninja was used directly,
#  `make` would just say it's up to date and do nothing
build/BUILD_TYPE_%:
	rm -rf build/
	$(MAKE) build/build.ninja
	touch $@

%/build.ninja:
	meson $(@D) $(MESON_OPTS)