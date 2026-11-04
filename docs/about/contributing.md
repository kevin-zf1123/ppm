[日本語](contributing.ja.md)

# Contributing

PPM is open-source software.
Contributions of any kind are welcome: fixes to the source code, translations, improvements to the manual, and so on.
Your contributions are released under the BSD 3-Clause License, as described in [License](license.md).

## How to contribute

The PPM repository is [opentoonz/ppm](https://github.com/opentoonz/ppm) on GitHub.

1. For large changes, discuss them in an [Issue](reporting-issues.md) first, so that your work is not wasted by a difference in direction.
2. Fork the repository, and make your changes on a working branch.
3. If you changed the source code, format it before committing (see "Coding style" below).
4. Open a pull request to the `main` branch.

## About pull requests

A pull request can be merged only when the reviewers can understand it.
As more people develop with AI, it has become easy to create pull requests that bundle many changes.
To keep reviews manageable, please help us with the following.

- **One pull request for one improvement.** If unrelated fixes are mixed in, we cannot merge only part of them.
- **If a change gets large, consider whether it can be split.** For example, put preparatory refactoring and the new feature in separate pull requests.
- **Write not only what you changed, but why it is needed and what the design intends.** For example, in what situation in production it helps, and why you chose this approach over others.
  If the code was written with AI, including the prompts you used also helps convey the intent.
- **If you changed or added a feature, also update the manual.** If you did not, say so in the pull request (see "Helping with the manual" below).

When you open a pull request, the description is filled in with a template for these points.
You do not need to get everything perfect. If you are unsure, feel free to ask in the pull request or an Issue.

If you develop with an AI coding tool, have it read `AGENTS.md` in the root of the repository. It summarizes the rules of this repository (the scope of changes, coding style, updating the manual, and so on). Claude Code reads it automatically through `CLAUDE.md`.

## Building from source

### Requirements

| Item | Details |
|---|---|
| OS | Windows 10 / 11 (64-bit) |
| Compiler | Visual Studio 2022 ("Desktop development with C++") |
| CMake | 3.21 or later |
| Qt | Qt 6.8 or later (MSVC 2022 64-bit). Qt 6.12 is used for development. When installing, also select the additional library **Qt 5 Compatibility Module**. |

The Qt modules used are Qt Widgets, Qt Network, Qt Core5Compat, Qt Concurrent, Qt QML, and Qt Linguist Tools.

### Build steps

Run the following in the repository folder.
For `QTDIR`, give the folder where Qt is installed (the folder that has `bin` and `lib`).
You can also set the environment variable `QTDIR` instead.

```bash
cmake -S sources -B build -G "Visual Studio 17 2022" -A x64 -DQTDIR=C:/Qt/6.12.0/msvc2022_64
```

```bash
cmake --build build --config Release
```

This creates `build/Release/ppm.exe`.
You can also open `ppm.sln` in the `build` folder in Visual Studio and build it there.

### Running it

To run `ppm.exe`, you need the Qt DLLs and `conf.ini`.

1. Use Qt's `windeployqt` to copy the required DLLs next to `ppm.exe`.

   ```bash
   C:/Qt/6.12.0/msvc2022_64/bin/windeployqt.exe build/Release/ppm.exe
   ```

2. Create `conf.ini` next to `ppm.exe`, and set the `stuff` folder of the repository as the Stuff folder (see [Installation](../getting-started/installation.md)).

   ```ini
   [General]
   StuffFolderPath=../../stuff
   ```

The `stuff` folder has the sample project and data.
To check that your build works, first follow the steps of the [Tutorial: Moving a Scene Through the Pipeline](../getting-started/tutorial.md).

### Building and running on macOS

Use Xcode Command Line Tools, CMake 3.21 or later, and a macOS Qt 6.8 or later installation with the modules listed above and Qt SVG. The supported macOS version and CPU architecture depend on the Qt installation and SDK used to build it. Set `QTDIR` to that Qt installation, or use `CMAKE_PREFIX_PATH`.

```bash
cmake -S sources -B build-macos -G Ninja -DQTDIR=/path/to/Qt/macos -DCMAKE_BUILD_TYPE=Release
cmake --build build-macos --target deploy_macos
open build-macos/macos/ppm.app
```

Install Ninja to use this generator, or omit `-G Ninja` to use the default generator. If the installed Qt and the default Xcode SDK are incompatible, select a compatible installed SDK with `-DCMAKE_OSX_SYSROOT=/path/to/MacOSX.sdk`.

The deployable app is `build-macos/macos/ppm.app`. CMake creates `conf.ini` beside `ppm.app`, pointing to the repository's `stuff` folder. Existing `conf.ini` files are preserved. For multi-configuration generators, the undeployed app and configuration are in the selected configuration directory (for example, `build-macos/Release`); deployment always writes `build-macos/macos`. The sample project writes progress and logs into `stuff/sample_data`; use a separate copy of Stuff when testing file operations.

The `deploy_macos` target builds the app, copies plugins from the same Qt installation used for linking, runs `macdeployqt`, and applies an ad-hoc signature for local execution. Run it again after rebuilding before moving the app to a Mac without Qt installed:

```bash
cmake --build build-macos --target deploy_macos
```

Distribute the resulting app together with `conf.ini` and a writable `stuff` folder outside the app; set `StuffFolderPath=stuff` in that configuration. Deployment includes Qt libraries and plugins; Developer ID signing and notarization are separate distribution steps. See [Qt's macOS deployment guide](https://doc.qt.io/qt-6/macos-deployment.html).

The macOS app icon is `sources/macos/ppm.icns`, generated from the existing `sources/ppm.ico` artwork under the project license. To regenerate it on macOS, run `python3 sources/macos/create_icon.py`; normal builds use the checked-in icon and do not require Python.

To run the focused platform behavior tests:

```bash
cmake -S sources -B build-macos -DPPM_BUILD_TESTS=ON
cmake --build build-macos --target ppm_platform_tests
ctest --test-dir build-macos -R '^ppm_platform_tests$' --output-on-failure
```

### Coding style

Before opening a pull request, always run clang-format on the C++ code you changed.
The formatting settings are in `sources/.clang-format`.

Run `beautification.sh` in the `sources` folder as follows (in Git Bash or a similar shell).

```bash
cd sources
./beautification.sh
```

This runs clang-format on all the `.cpp` and `.h` files changed from the `main` branch.
You can use the clang-format included in Visual Studio or the one from LLVM. Put it in a folder on your PATH.

Write the text shown on the screen in English, and wrap it in `tr()`. The Japanese translation goes into the translation file (see below).

## Helping with translations

You can add UI languages to PPM with translation files.
How to make a translation file is described in "Adding a language" in [UI Language and Translation Files](../administration/translations.md).

- If you want your translation included in PPM, send the translated `.ts` file (`sources/translations/PPM_<language code>.ts`) in a pull request. For a new language, also add the language code to `LANGUAGES` in `sources/CMakeLists.txt`.
- Reports of mistakes or unclear wording in the existing (Japanese) translation are also welcome.

## Helping with the manual

This manual is a set of Markdown files in the `docs` folder of the repository.

- Each page has a Japanese version (`*.ja.md`) and an English version (`*.md`). When you change one, change the other as well. If you can change only one, say so in the pull request, and we will update the other.
- Screenshots go in `docs/images/ja`. The English pages also use the Japanese screenshots.
- The manual describes how PPM behaves now. If you think something behaves wrongly, report it in an [Issue](reporting-issues.md) instead of changing the manual.

---

[← Back to contents](../index.md)
