[日本語](installation.ja.md)

# Installation

PPM does not use an installer. On Windows, extract the distributed ZIP file and start `ppm.exe` inside it. For macOS, see the app layout below and the [build instructions](../about/contributing.md#building-and-running-on-macos).

## System requirements

| Item | Requirement |
|---|---|
| OS | Windows 10 / 11 (64-bit), or macOS compatible with the Qt libraries and architecture used in the build |
| Network | Each PC can access the shared folder (file server or NAS) where the scene files are stored |
| Applications for the work (optional) | CLIP STUDIO PAINT, TVPaint Animation, OpenToonz, a timesheet application that can open XDTS files, and so on. They are used to open the files of each scene from the PPM list |

PPM itself works without these applications.
If you do not set the path of an application in PPM, the file is opened with the application associated with its file extension (see [Initial Setup](initial-setup.md)).

## Windows distribution

<!-- To be confirmed: once the distribution format (e.g. a ZIP on GitHub Releases) is decided, add the download link and the file name -->

When you extract the distributed ZIP file, you get the following structure.

```
PPM/
├─ ppm.exe             … PPM itself
├─ conf.ini            … Settings file that specifies the location of the Stuff folder
├─ README.txt          … Read-me file (with links to the manual, and so on)
├─ Qt6Core.dll, etc.   … Libraries required to run PPM (Qt)
├─ platforms/, etc.    … Same as above
└─ stuff/              … The Stuff folder (projects, translation files, and so on)
   ├─ projects/        … Project files (.ppmprj)
   ├─ loc/             … Translation files (UI languages)
   ├─ rename_scripts/  … Rename scripts
   ├─ sample_data/     … Sample data for the tutorial
   └─ LICENSE/         … License documents
```

To install PPM:

1. Extract the ZIP file to any folder.
   (For example, `C:\Tools\PPM`. Avoid `C:\Program Files`, where writing requires administrator rights.)
2. Double-click `ppm.exe` to start it.

To uninstall PPM, delete the extracted folder.
If you also want to remove the per-user settings (made in [Initial Setup](initial-setup.md)),
delete `%LOCALAPPDATA%\PPM\usersettings.ini` as well.

## macOS app layout

Place these items in the same writable folder, and open `ppm.app`:

```text
PPM/
├─ ppm.app
├─ conf.ini
└─ stuff/
```

Keep `conf.ini` and Stuff outside the app bundle. With `StuffFolderPath=stuff`, PPM resolves Stuff relative to the folder containing `ppm.app`, independent of the working directory used to launch it. Moving the app alone does not move its configuration or production data. Preferences are stored in `~/Library/Preferences/PPM/usersettings.ini`; deleting this file removes the per-user settings.

Mount SMB shares in Finder before opening a project, and use their local paths such as `/Volumes/Production/PPM/stuff`. Windows drive letters and UNC paths do not map automatically to macOS. For projects shared between Windows and Mac, use paths relative to Stuff wherever possible; set machine-specific application paths in Preferences. Project file and log formats are shared between the two platforms.

On macOS, **Zip Folder** uses the system archive utility and stores the source folder's contents, including dotfiles, without adding macOS resource forks. The ZIP destination must be outside the source folder. Undo deletes the ZIP, as on Windows.

## conf.ini and the Stuff folder

The **Stuff folder** contains the project files, translation files, rename scripts, and other files used by PPM.
At startup, PPM reads `conf.ini` beside `ppm.exe` on Windows or beside `ppm.app` on macOS to find the Stuff folder.

```ini
[General]
StuffFolderPath=stuff
```

- A relative path is relative to the folder containing `conf.ini`. In the example above, its neighboring `stuff` folder is the Stuff folder.
- You can also specify a Stuff folder on a shared folder.
  If everyone in the studio uses the same Stuff folder, the project settings can be managed in one place.
  In that case, the easiest way is to use forward slashes (`/`) as path separators (e.g. `StuffFolderPath=//fileserver/anime/PPM/stuff`).
- In `conf.ini`, a backslash (`\`) is an escape character that is combined with the next character.
  For example, `\t` turns into a tab, and in many other combinations such as `\P`, both characters disappear.
  To use backslashes, write each one twice as `\\` (e.g. `StuffFolderPath=\\\\fileserver\\anime\\PPM\\stuff`).
  Enclosing the value in double quotes does not change this.

For the structure of the Stuff folder and how to run PPM in a studio, see [The Stuff Folder](../administration/stuff-folder.md) and [Running PPM on a Shared Folder](../administration/shared-operation.md).

---

[← Back to contents](../index.md)
