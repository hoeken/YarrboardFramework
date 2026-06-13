# v3.0.3

A small maintenance release with frontend formatting tweaks, a debug logging guard, and coredump tooling fixes.

## 🔧 Improvements

### Frontend & UI
- `formatBytes()` now supports automatic resolution: passing no `decimals` argument shows ~3 significant digits, scaling precision to the magnitude of the value
- Memory/heap/PSRAM stats on the system page now use the new automatic resolution instead of forcing zero decimals

### Firmware
- `esp_log_set_vprintf()` redirection in `DebugController` is now guarded behind the `YB_DEBUG_VPRINTF` flag

## 🐛 Bug Fixes

### Tooling
- Fixed ELF file path in `parse_coredump.py` to match the `docs/releases/{board}/{version}.elf` release layout
- Changed the default coredump file argument from `coredump.txt` to `coredump.bin`