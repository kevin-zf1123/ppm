# AGENTS.md

Guidelines for AI coding agents (and humans) working in this repository.
For the contribution process, see [CONTRIBUTING.md](CONTRIBUTING.md).

## About PPM

PPM is a Windows desktop application (C++ / Qt 6) for managing the progress of animation production.
It decides which workflow each scene is in from the names and locations of files on a shared folder, and runs file operations (rename, copy, and so on) when users press action buttons.
Many people use it at the same time on a file server, so data written by PPM must stay compatible and safe.

## Repository layout

| Path | Contents |
|---|---|
| `sources/` | C++ source code, `CMakeLists.txt`, Qt resources (`ppm_resources.qrc`, `Resources/`) |
| `sources/translations/PPM_ja.ts` | Japanese translation source |
| `stuff/` | The Stuff folder: projects (`projects/`), translation files (`loc/`), rename scripts, sample data, license texts (`LICENSE/`) |
| `docs/` | The user manual (Markdown). Each page has a Japanese (`*.ja.md`) and an English (`*.md`) version |
| `google_apps_script/` | Google Apps Script for the Google Spreadsheet and Google Chat integrations |
| `LICENSE` | The license of PPM (BSD 3-Clause). Copied to `stuff/LICENSE/LICENSE.txt` at build time |

## Build

Requirements: Windows, Visual Studio 2022, CMake 3.5 or later, Qt 6 (MSVC 2022 64-bit, with the Qt 5 Compatibility Module).

```bash
cmake -S sources -B build -G "Visual Studio 17 2022" -A x64 -DQTDIR=C:/Qt/6.12.0/msvc2022_64
cmake --build build --config Release
```

To run the built `ppm.exe`, see "Building from source" in [docs/about/contributing.md](docs/about/contributing.md).

## Rules

### Scope of changes

- Make one improvement per pull request. Do not mix unrelated fixes, refactoring, or reformatting of untouched code.
- Keep the diff as small as the change needs. If a change gets large, propose splitting it.
- Do not change the formats of files that PPM reads and writes (`.ppmprj`, the INI files in the project info folder, logs) without discussing it in an Issue first. Existing projects on file servers must keep working.
- When behavior is unclear, check the existing code and the manual first. If the code and the manual disagree, report it instead of silently changing either one.

### Code

- Before committing, format the changed C++ code: `cd sources` then `./beautification.sh` (clang-format with `sources/.clang-format`).
- Write UI text in English and wrap it in `tr()`. Add the Japanese translation to `sources/translations/PPM_ja.ts`, and regenerate `stuff/loc/PPM_ja.qm` (CMake target `translation_PPM`, or `lupdate` / `lrelease`).
- Code comments may be in Japanese or English. Follow the style of the surrounding code.
- Keep the encoding (UTF-8) and the line endings of each file you edit.
- The version number is defined only in `project(ppm VERSION ...)` in `sources/CMakeLists.txt`.
- New icons must be your own work or under a license compatible with BSD 3-Clause (e.g. MIT). Record the source in the file, and add the license text to `stuff/LICENSE/` and to the About dialog (`sources/aboutpopup.cpp`). Do not use icons from sites that prohibit redistribution of the files.

### Manual

- If you change or add a feature, update the manual in `docs/`: both the Japanese (`*.ja.md`) and the English (`*.md`) pages.
- The manual describes how PPM behaves now. If the behavior seems wrong, report it rather than describing the intended behavior.
- Screenshots go in `docs/images/ja/`. English pages also use these images. Step icons are in `docs/images/icons/`.

### Sample data

- `stuff/projects/sample_project.ppmprj` and `stuff/sample_data/` are used by the tutorial in the manual. Keep them consistent with the manual.
- Running PPM with the sample project rewrites files in `stuff/sample_data/` (progress lists, logs, and so on). Do not commit these changes unless the change is intended.

### Do not commit

- `build/` and other build outputs
- `stuff/LICENSE/LICENSE.txt` (generated from `LICENSE` at build time)

## Pull request description

Fill in the pull request template:

- what you changed
- why it is needed
- the design intent
- how you tested it
- if you used an AI coding tool, which tool you used and the prompts you used

Also state whether you updated the manual.
