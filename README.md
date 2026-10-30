# Armulators

Code shared by armGBA and armN64.

- `emit/a64`, `emit/x64`, `emit/a32`: the AArch64, x86-64 and ARMv7 code generators the
  recompilers write with (header-only).
- `libretro/`: libretro.h and what a libretro core needs beside its emulator: core options for new
  and old frontends (`options`), soft patches beside the game (`patch`, IPS/UPS/BPS) and worker
  threads (`workers`). `link.T` keeps a core's exports to `retro_*`.

An emulator takes it as a submodule and adds it with `add_subdirectory`:

```cmake
add_subdirectory(armulators)
target_link_libraries(cpu PRIVATE armulators-emit)
target_link_libraries(core_libretro PRIVATE armulators-libretro)
target_link_options(core_libretro PRIVATE -Wl,--version-script=${ARMULATORS_LINK_SCRIPT})
```

Licensed under the Mozilla Public License 2.0, except `libretro/libretro.h` (MIT, its own header).
