cmake_minimum_required(VERSION 3.13)
project(armulators C)

# Code generators: header-only, included as "a64/emit.h", "x64/emit.h" or "a32/emit.h".
add_library(armulators-emit INTERFACE)
target_include_directories(armulators-emit INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/emit)

# Pieces a libretro core shares: options, soft patches and worker threads.
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
add_library(armulators-libretro STATIC libretro/options.c libretro/patch.c libretro/workers.c)
target_include_directories(armulators-libretro PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libretro)
target_link_libraries(armulators-libretro PUBLIC ZLIB::ZLIB Threads::Threads)
set_target_properties(armulators-libretro PROPERTIES C_STANDARD 11 POSITION_INDEPENDENT_CODE ON)
# The version script that keeps a core's exports to retro_*.
get_directory_property(ARMULATORS_PARENT PARENT_DIRECTORY)
if(ARMULATORS_PARENT)
	set(ARMULATORS_LINK_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/libretro/link.T PARENT_SCOPE)
endif()

# The standalone app, for an emulator to build with armulators_app() (app/app.cmake).
include(app/app.cmake)
