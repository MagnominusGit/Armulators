#!/bin/sh

# Copyright (c) 2026 Magnominus
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# build.sh [emulator]: builds an emulator's standalone app (by default the one this is a submodule of)
# for x86-64 and 64-bit ARM in the Debian 11 container (Dockerfile), into build-appimage/<arch>/ in
# the emulator's folder. package.sh then makes the AppImages.
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "${1:-$here/../../..}" && pwd)
out=$root/build-appimage
mkdir -p "$out"
cmake -DDESCRIPTOR="$root/app.cmake" -DOUT="$out/app.sh" -P "$here/../describe.cmake"
. "$out/app.sh"
docker build -q -t armulators-appimage "$here" > /dev/null
docker run --rm -u "$(id -u):$(id -g)" -v "$root:/src" -w /src -e APP_ID="$APP_ID" -e OPTION="$APP_BUILD_OPTION" -e TARGET="$APP_BUILD_TARGET" \
	-e TOOLCHAIN="/src/${here#"$root"/}/aarch64.cmake" armulators-appimage sh -c '
	set -e
	for arch in x86_64 aarch64; do
		toolchain=
		[ $arch = aarch64 ] && toolchain=-DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN
		cmake -S . -B build-appimage/build-$arch $toolchain -DCMAKE_BUILD_TYPE=Release -D$OPTION=ON \
			-DSDL2_INCLUDE_DIR=/opt/sdl2-$arch/include/SDL2 \
			"-DSDL2_LIBRARY=/opt/sdl2-$arch/lib/libSDL2.a;-ldl;-lrt;-lpthread;-static-libstdc++;-static-libgcc" > /dev/null
		cmake --build build-appimage/build-$arch -j"$(nproc)" --target $TARGET > build-appimage/build-$arch.log 2>&1 || { tail -20 build-appimage/build-$arch.log; exit 1; }
		mkdir -p build-appimage/$arch
		cp build-appimage/build-$arch/$APP_ID build-appimage/$arch/$APP_ID
		strip -s build-appimage/$arch/$APP_ID 2>/dev/null || $([ $arch = aarch64 ] && echo aarch64-linux-gnu-strip) -s build-appimage/$arch/$APP_ID
		echo "$arch: $(file -b build-appimage/$arch/$APP_ID | cut -c1-80)"
	done
'
