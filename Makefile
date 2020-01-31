all: build/build.ninja
	ninja -C $(<D)

build/build.ninja:
	meson $(@D)
