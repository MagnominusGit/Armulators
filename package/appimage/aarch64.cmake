# Copyright (c) 2026 Magnominus
#
# SPDX-License-Identifier: GPL-3.0-only
# Licensed under the GNU General Public License, version 3: see LICENSE.

# Cross-building for 64-bit ARM Linux in the AppImage container.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# Debian's arm64 zlib, which is outside the root above.
set(ZLIB_LIBRARY /usr/lib/aarch64-linux-gnu/libz.so)
set(ZLIB_INCLUDE_DIR /usr/include)
