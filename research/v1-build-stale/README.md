# Evidence — stale V1 build configuration

`CMakeCache-excerpt-554-574.txt` is an excerpt of `/home/towfienes/Projects/HyprVX/build/CMakeCache.txt`
(lines 554–574) captured 2026-10-10. It shows `HYPRLAND_PREFIX=/tmp/hyprland-src` and include dirs under
`/tmp/hyprland-src/{protocols,src}`; that directory no longer exists. The V1 plugin object was compiled against
those headers, so the reported "298/298 PASS" came from pre-existing binaries and does **not** prove a clean
rebuild. `CMakeLists.txt` itself contains no `/tmp/hyprland-src` reference; `pkg-config --cflags hyprland` now
resolves to `/usr/include/hyprland{,/protocols,/src}`. Fix: configure a fresh build directory (never reuse `build/`).
The V1 `build/` directory was not modified.
