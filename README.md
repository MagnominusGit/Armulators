# Armulators

Code shared by armGBA and armN64.

- `emit/a64`, `emit/x64`, `emit/a32`: the AArch64, x86-64 and ARMv7 code generators the
  recompilers write with (header-only).
- `libretro/`: libretro.h and what a libretro core needs beside its emulator: core options for new
  and old frontends (`options`), soft patches beside the game (`patch`, IPS/UPS/BPS) and worker
  threads (`workers`). `link.T` keeps a core's exports to `retro_*`.
- `app/`: the standalone app, a libretro core linked into an SDL2 program, with menus, save states,
  rewind, netplay, cheats, achievements and on-screen controls. An emulator names it and its systems
  in an `app.cmake` and adds what only it needs through hooks (`app-hooks.h`).
- `package/`: the AppImage (x86-64 and 64-bit ARM) and Android APK recipes, which read the
  emulator's `app.cmake`, so each emulator's artifacts are made the same way.
- `deps/`: libretro-common, the LZMA SDK's decoder and rcheevos, as RetroArch carries them (see its
  README).

An emulator takes it as a submodule and adds it with `add_subdirectory`:

```cmake
add_subdirectory(armulators)
target_link_libraries(cpu PRIVATE armulators-emit)
target_link_libraries(core_libretro PRIVATE armulators-libretro)
target_link_options(core_libretro PRIVATE -Wl,--version-script=${ARMULATORS_LINK_SCRIPT})
```

The app, from the emulator's own CMakeLists.txt, with its core's sources and its hooks:

```cmake
option(EMU_APP "Build the standalone app" OFF)
if(EMU_APP)
	add_subdirectory(armulators)
	armulators_app(emu-app DESCRIPTOR ${CMAKE_CURRENT_SOURCE_DIR}/app.cmake
		SOURCES ${LIBRETRO_SOURCES} app/hooks.c INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/libretro LIBS emu)
endif()
```

and its AppImages and APK, from the emulator's folder: `armulators/package/appimage/build.sh`, then
`package.sh` beside it, and `armulators/package/android/build.sh`.

Licensed under the GNU General Public License, version 2 or (at your option) any later version (`LICENSE`), except `libretro/libretro.h` (MIT, its own header),
`app/stb_truetype.h` (public domain or MIT), the fonts in `app/fonts/` (SIL Open Font License 1.1,
`LICENSES/OFL-1.1.txt`) and `deps/` (MIT and public domain, as each file states).
