# The standalone app (app/): a libretro core linked into an SDL2 program, with menus, netplay,
# achievements and the rest. An emulator builds its own with
#
#   armulators_app(target DESCRIPTOR app.cmake SOURCES core and hooks .c files
#                  INCLUDES the core's folders LIBS the core's libraries [ARCHIVE target])
#
# The descriptor names the app (the program is APP_ID) and its systems; app-info.h.in has what each
# value is for. The hooks (app-hooks.h) define appHooks. ARCHIVE is a target with libretro-common's
# archive reading when the core has one already; the app builds its own from deps/ otherwise. SDL2
# comes from pkg-config, or SDL2_INCLUDE_DIR and SDL2_LIBRARY when given.

set(ARMULATORS_APP_DIR ${CMAKE_CURRENT_LIST_DIR} CACHE INTERNAL "")
set(ARMULATORS_DEPS_DIR ${CMAKE_CURRENT_LIST_DIR}/../deps CACHE INTERNAL "")

function(armulators_app TARGET)
	cmake_parse_arguments(ARG "" "DESCRIPTOR;ARCHIVE" "SOURCES;INCLUDES;LIBS" ${ARGN})
	set(app ${ARMULATORS_APP_DIR})
	set(deps ${ARMULATORS_DEPS_DIR})
	if(NOT SDL2_LIBRARY)
		find_package(PkgConfig REQUIRED)
		pkg_check_modules(SDL2 REQUIRED sdl2)
		set(SDL2_INCLUDE_DIR ${SDL2_INCLUDE_DIRS})
		set(SDL2_LIBRARY ${SDL2_LINK_LIBRARIES})
	endif()

	# RetroArch's archive reading, and RetroAchievements' library, which takes its MD5 from it.
	set(archive ${ARG_ARCHIVE})
	if(NOT archive)
		set(archive armulators-archive)
		if(NOT TARGET armulators-archive)
			file(GLOB_RECURSE ARCHIVE_SOURCES ${deps}/libretro-common/*.c ${deps}/7zip/*.c)
			add_library(armulators-archive STATIC ${ARCHIVE_SOURCES})
			target_include_directories(armulators-archive PUBLIC ${deps}/libretro-common/include PRIVATE ${deps})
			target_compile_definitions(armulators-archive PRIVATE HAVE_ZLIB HAVE_7ZIP)
			target_compile_options(armulators-archive PRIVATE -w -O2)
		endif()
	endif()
	if(NOT TARGET armulators-rcheevos)
		file(GLOB_RECURSE RCHEEVOS_SOURCES ${deps}/rcheevos/src/*.c)
		add_library(armulators-rcheevos STATIC ${RCHEEVOS_SOURCES})
		target_include_directories(armulators-rcheevos PUBLIC ${deps}/rcheevos/include ${deps}/rcheevos/src)
		target_link_libraries(armulators-rcheevos PRIVATE ${archive})
		target_compile_options(armulators-rcheevos PRIVATE -w -O2)
	endif()

	# The fonts as C arrays.
	set(out ${CMAKE_CURRENT_BINARY_DIR}/${TARGET})
	set(FONTS ${app}/fonts/Inter-Medium.otf ${app}/fonts/Inter-SemiBold.otf ${app}/fonts/ChakraPetch-Bold.ttf)
	string(REPLACE ";" "|" FONT_LIST "${FONTS}")
	add_custom_command(OUTPUT ${out}/fonts.inc
		COMMAND ${CMAKE_COMMAND} -DFONTS=${FONT_LIST} -DOUT=${out}/fonts.inc -P ${app}/fonts/embed.cmake
		DEPENDS ${FONTS} ${app}/fonts/embed.cmake VERBATIM)

	# The descriptor's values, its lists as C initializers, into app-info.h.
	include(${ARG_DESCRIPTOR})
	string(REPLACE "." "/" APP_JAVA_PATH ${APP_JAVA_PACKAGE})
	list(TRANSFORM APP_EXTENSIONS PREPEND "\".")
	list(TRANSFORM APP_EXTENSIONS APPEND "\"")
	list(JOIN APP_EXTENSIONS ", " APP_EXTENSION_LIST)
	list(JOIN APP_ARCHIVED "|" APP_ARCHIVED_LIST)
	list(GET APP_WINDOW 0 APP_WINDOW_WIDTH)
	list(GET APP_WINDOW 1 APP_WINDOW_HEIGHT)
	list(LENGTH APP_SYSTEMS APP_SYSTEM_COUNT)
	set(APP_SYSTEM_KEY_LIST "")
	set(APP_SYSTEM_NAME_LIST "")
	set(APP_SYSTEM_FOLDER_LIST "")
	foreach(key ${APP_SYSTEMS})
		list(GET APP_SYSTEM_${key} 0 name)
		list(SUBLIST APP_SYSTEM_${key} 1 -1 aliases)
		list(TRANSFORM aliases PREPEND "\"")
		list(TRANSFORM aliases APPEND "\"")
		list(JOIN aliases ", " aliases)
		string(APPEND APP_SYSTEM_KEY_LIST "\"${key}\", ")
		string(APPEND APP_SYSTEM_NAME_LIST "\"${name}\", ")
		string(APPEND APP_SYSTEM_FOLDER_LIST "{ \"${key}\", ${aliases}, NULL }, ")
	endforeach()
	# The keys as SDL's scancodes, NONE for none; the stick keys only when there are 24.
	list(LENGTH APP_KEYS count)
	set(APP_STICK_KEYS 0)
	if(count EQUAL 24)
		set(APP_STICK_KEYS 1)
	endif()
	list(TRANSFORM APP_KEYS REPLACE "^NONE$" "UNKNOWN")
	list(TRANSFORM APP_KEYS PREPEND "SDL_SCANCODE_")
	list(JOIN APP_KEYS ", " APP_KEY_LIST)
	if(NOT APP_BUTTON_NAMES)
		set(APP_BUTTON_NAMES B "Turbo B" Select Start Up Down Left Right A "Turbo A" L R L2 R2 L3 R3
			"Left Stick Up" "Left Stick Down" "Left Stick Left" "Left Stick Right"
			"Right Stick Up" "Right Stick Down" "Right Stick Left" "Right Stick Right")
	endif()
	set(APP_BUTTON_NAME_LIST "")
	foreach(name IN LISTS APP_BUTTON_NAMES)
		if(name STREQUAL "-")
			string(APPEND APP_BUTTON_NAME_LIST "NULL, ")
		else()
			string(APPEND APP_BUTTON_NAME_LIST "\"${name}\", ")
		endif()
	endforeach()
	configure_file(${app}/app-info.h.in ${out}/app-info.h @ONLY)

	file(GLOB APP_SOURCES ${app}/*.c)
	# On Android it is libmain.so, which SDL's activity loads and starts.
	if(ANDROID)
		add_library(${TARGET} SHARED ${APP_SOURCES} ${ARG_SOURCES} ${out}/fonts.inc)
		set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME main)
	else()
		add_executable(${TARGET} ${APP_SOURCES} ${ARG_SOURCES} ${out}/fonts.inc)
		set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME ${APP_ID})
	endif()
	target_include_directories(${TARGET} PRIVATE ${app} ${ARG_INCLUDES} ${SDL2_INCLUDE_DIR} ${out})
	target_link_libraries(${TARGET} ${ARG_LIBS} armulators-libretro armulators-rcheevos ${archive} ${SDL2_LIBRARY} m z ${CMAKE_DL_LIBS})
	target_compile_options(${TARGET} PRIVATE -O2)
endfunction()
