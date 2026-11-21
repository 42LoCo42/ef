run: build
	./build/example

build:
	#!/usr/bin/env bash
	if [ ! -d build ] || [ ! -f build/meson-private/coredata.dat ]; then just _wipe; fi
	meson compile -v -C build

wipe: _wipe
	just run

_wipe:
	meson setup --wipe build
