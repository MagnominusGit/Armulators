# Copyright (c) 2026 Magnominus
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# cmake -DDESCRIPTOR=app.cmake -DOUT=file [-DIN=template] -P describe.cmake: an emulator's app.cmake
# written into a template, or, without one, as shell lines (KEY='value') for the package scripts.
include(${DESCRIPTOR})
string(REPLACE "." "/" APP_JAVA_PATH ${APP_JAVA_PACKAGE})
list(JOIN APP_MIME ";" APP_MIME_LIST)
if(IN)
	configure_file(${IN} ${OUT} @ONLY)
else()
	set(lines "")
	foreach(key APP_ID APP_NAME APP_VERSION APP_BUILD_OPTION APP_BUILD_TARGET APP_JAVA_PACKAGE APP_JAVA_PATH APP_JAVA_CLASS)
		string(APPEND lines "${key}='${${key}}'\n")
	endforeach()
	file(WRITE ${OUT} "${lines}")
endif()
