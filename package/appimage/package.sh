#!/bin/sh

# Copyright (c) 2026 Magnominus
#
# SPDX-License-Identifier: GPL-2.0-or-later
# Licensed under the GNU General Public License, version 2 or (at your option) any later version: see LICENSE.

# package.sh [emulator]: makes build-appimage/<name>-x86_64.AppImage and <name>-aarch64.AppImage
# from what build.sh built, with the emulator's app/icon.png.
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "${1:-$here/../../..}" && pwd)
out=$root/build-appimage
cmake -DDESCRIPTOR="$root/app.cmake" -DOUT="$out/app.sh" -P "$here/../describe.cmake"
. "$out/app.sh"
cmake -DDESCRIPTOR="$root/app.cmake" -DIN="$here/app.desktop.in" -DOUT="$out/$APP_ID.desktop" -P "$here/../describe.cmake"
cp "$root/app/icon.png" "$out/$APP_ID.png"
docker build -q -t armulators-appimage "$here" > /dev/null
docker run --rm -u "$(id -u):$(id -g)" -v "$out:/out" -w /out -e APP_ID="$APP_ID" -e APP_NAME="$APP_NAME" armulators-appimage sh -c '
	set -e
	for arch in x86_64 aarch64; do
		rm -rf AppDir-$arch
		mkdir -p AppDir-$arch/usr/bin AppDir-$arch/usr/share/applications AppDir-$arch/usr/share/icons/hicolor/256x256/apps
		cp $arch/$APP_ID AppDir-$arch/usr/bin/
		cp $APP_ID.desktop AppDir-$arch/usr/share/applications/
		cp $APP_ID.png AppDir-$arch/usr/share/icons/hicolor/256x256/apps/
		cp $APP_ID.desktop $APP_ID.png AppDir-$arch/
		ln -s usr/bin/$APP_ID AppDir-$arch/AppRun
		ARCH=$arch /opt/appimage/appimagetool --appimage-extract-and-run --no-appstream --runtime-file /opt/appimage/runtime-$arch \
			AppDir-$arch $APP_NAME-$arch.AppImage > package-$arch.log 2>&1 || { tail -20 package-$arch.log; exit 1; }
		rm -rf AppDir-$arch
		echo "$(ls -la $APP_NAME-$arch.AppImage | awk "{print \$5}") bytes  $APP_NAME-$arch.AppImage"
	done
'
