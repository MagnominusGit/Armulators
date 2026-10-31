# What the app takes from RetroArch

- `libretro-common/`: the files RetroArch's archive reading needs, so the app opens the same `.zip`
  and `.7z` files RetroArch does, and its MD5, which rcheevos uses. From RetroArch
  bdba046fa6766380bc2457532f38e589df769aaf (v1.22.2 and fixes). MIT license, stated in each file.
- `7zip/`: the LZMA SDK's decoder, from RetroArch's `deps/7zip` at the same commit. Public domain.
- `rcheevos/`: RetroAchievements' library, v12.3.0 as RetroArch carries it in `deps/rcheevos` at the
  same commit, including RetroArch's `src/rhash/md5.h`. MIT license.

The files are unchanged. Zip support uses the system's zlib. An emulator whose core already has
libretro-common's archive reading gives the app its target instead (`ARCHIVE`).
