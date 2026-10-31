# Copyright (c) 2026 Magnominus
#
# SPDX-License-Identifier: GPL-2.0-or-later
# Licensed under the GNU General Public License, version 2 or (at your option) any later version: see LICENSE.

# cmake -DDESCRIPTOR=app.cmake -DOUT=file [-DIN=template] -P describe.cmake: an emulator's app.cmake
# written into a template, or, without one, as shell lines (KEY='value') for the package scripts.
include(${DESCRIPTOR})
string(REPLACE "." "/" APP_JAVA_PATH ${APP_JAVA_PACKAGE})
list(JOIN APP_MIME ";" APP_MIME_LIST)
list(JOIN APP_BUILD_ARGS " " APP_BUILD_ARGS)
if(IN)
	configure_file(${IN} ${OUT} @ONLY)
else()
	set(lines "")
	foreach(key APP_ID APP_NAME APP_VERSION APP_BUILD_OPTION APP_BUILD_TARGET APP_BUILD_ARGS APP_JAVA_PACKAGE APP_JAVA_PATH APP_JAVA_CLASS)
		string(APPEND lines "${key}='${${key}}'\n")
	endforeach()
	file(WRITE ${OUT} "${lines}")
endif()
