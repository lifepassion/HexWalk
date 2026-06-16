# Repository Guidelines

## Project Structure & Module Organization

HexWalk is a Qt/qmake C++ desktop application. Main application code lives in `hexwalk/`, with each dialog or feature split across `.cpp`, `.h`, and `.ui` files. Shared hex editor code is vendored in `qhexedit/` and included by `hexwalk/hexwalk.pro`. Capstone is a Git submodule in `capstone/` and is linked as a static library for disassembly support. Binary header patterns are YAML files in `patterns/`. Images, fonts, Qt resources, and translations are under `hexwalk/images/`, `hexwalk/fonts/`, `hexwalk/hexwalk.qrc`, and `hexwalk/translations/`. Debian packaging is isolated in `deb-packaging/`; Windows binwalk support is under `binwalk_windows/`.

## Build, Test, and Development Commands

- `git submodule update --init --recursive`: fetch the Capstone submodule after cloning.
- `make -C capstone`: build `capstone/libcapstone.a`, required by `hexwalk/hexwalk.pro` on Unix-like systems.
- `./linux_build.sh`: clean `build/`, run `qmake-qt5 ../hexwalk/hexwalk.pro`, and compile the app into `bin/`.
- `mkdir -p build && cd build && qmake-qt5 ../hexwalk/hexwalk.pro && make`: manual build flow when inspecting generated Makefiles or passing extra `make` flags.
- `./bin/hexwalk [file]`: run the built application, optionally opening a binary file.

## Coding Style & Naming Conventions

Follow the existing Qt C++ style: 4-space indentation, braces on their own lines for function/control blocks, Qt containers and types (`QString`, `QByteArray`, `QSettings`) where already used, and signal/slot wiring consistent with surrounding code. Keep feature classes named by UI responsibility, such as `SearchDialog`, `ByteMapDialog`, or `ConverterWidget`. When adding Qt forms, update `hexwalk/hexwalk.pro` `SOURCES`, `HEADERS`, `FORMS`, and `RESOURCES` as needed.

## Testing Guidelines

There is no dedicated HexWalk test suite in this repository. Before submitting changes, run a clean build with `./linux_build.sh` and manually exercise affected workflows in `./bin/hexwalk`: file open/save, search, entropy, byte map, disassembly, tags, diff, or pattern parsing as relevant. For parser or pattern changes, verify with a small representative binary and the matching file in `patterns/`.

## Commit & Pull Request Guidelines

Recent history uses short, imperative commit subjects such as `Remove 64-byte width limit for hex display` and `Use qmake-qt5 for building hexwalk project`. Keep commits focused and avoid mixing vendored dependency changes with application changes. Pull requests should describe the user-visible behavior, list build/manual test results, link related issues, and include screenshots for UI changes.

## Security & Configuration Tips

Treat opened binaries and binwalk output as untrusted input. Avoid shelling out with unsanitized file paths, keep generated artifacts in `build/` or `bin/`, and do not commit local Qt Creator settings or system-specific binaries.
