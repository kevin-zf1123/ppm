[日本語](shared-operation.ja.md)

# Running PPM on a Shared Folder

PPM manages progress with files, not with a database. You do not need to set up a dedicated server or database for PPM.
Everyone in the studio shares progress because the PPM on each PC directly reads and writes the same files in a shared folder on a file server or NAS (see [Progress Is Managed by Files](../concepts/progress-files.md)).
This page describes the setup for this and what to watch out for when several people use PPM.

## Recommended setup

### What to put in the shared folder

Put all of the following folders in the shared folder, so that everyone using PPM sees the same ones.

| Folder | Where it is set |
|---|---|
| Stuff folder | `conf.ini` on each PC (see [The Stuff Folder](stuff-folder.md)) |
| Root folder (scene files) | **Basics** in the project settings |
| Project info folder | Same as above |
| Log folder (log root folder) | Same as above |

An example layout:

```
\\fileserver\anime\
├─ PPM\
│  └─ stuff\                 … Stuff folder
│     ├─ projects\
│     ├─ loc\
│     └─ rename_scripts\
└─ my_title\
   ├─ cuts\                  … root folder
   └─ PPM\
      ├─ info\               … project info folder
      └─ log\                … log folder
```

### Use paths that look the same from every PC

The paths written in the project settings and in `conf.ini` must point to the same place on every PC.

- Paths that start with the server name (UNC paths), such as `\\fileserver\anime\...`, are the most reliable. In the project settings you can also write them as `//fileserver/anime/...`.
- Paths with drive letters, such as `Z:\anime\...`, can be used only if every PC maps the same drive letter.
- If you write the root folder and other folders as relative paths, they are relative to the Stuff folder. If the Stuff folder and the scene files are on the same server, relative paths mean less to rewrite when the server name changes.

### Where to put PPM itself

PPM itself (the folder with `ppm.exe`) can be put on each PC or in the shared folder.

| Location | Advantages | Things to watch out for |
|---|---|---|
| Each PC (e.g. `C:\Tools\PPM`) | Starts quickly. PPM itself can start even if the server is down. | Write the shared Stuff folder in `conf.ini` on each PC. To update PPM, replace it on every PC. |
| Shared folder | Everyone always uses the same version. Only one `conf.ini` is needed. | To update PPM, everyone has to quit PPM (a `ppm.exe` in use cannot be overwritten). |

In either case, **make sure everyone uses the same version of PPM**.
If versions are mixed, an older version may not read correctly the project settings saved by a newer version.

## Several people at the same time

### Running actions

Any number of people can run actions at the same time, as long as they are on different scenes.

- Actions run by others are not reflected in your list automatically. Press **Refresh** to reflect them (see [Which Scenes Appear in the List](../using/scene-list.md)).
- If two people run an action on the same scene at almost the same time, the steps of the one who runs it later may fail because the files have already been moved. If this happens, press **Refresh** and check which workflow the scene is in now (see "When an action fails" in [How Scenes Flow Through Workflows](../concepts/scene-flow.md)).
- Someone else may move the same scene forward while your confirmation dialog is open. Do not leave the confirmation dialog open for a long time.

The scene list of each workflow (the files in the `progress_cache` folder) is protected from being rewritten by two people at once, but only while it is being rewritten.
During that time, a file named `..._details.lock` is created temporarily in the same folder.
It may remain when PPM is forced to quit, but after a while PPM ignores it and continues.

### Changing the project settings

Changes to the project settings are not reflected in other people's PPM right away.
PPM reads the project file **only when it starts and when you switch projects**.

- After saving the project settings, ask the staff to restart PPM. New projects also appear in the **Project** list after a restart.
- Only one person at a time should edit the project settings. PPM does not tell you that someone else is editing them at the same time. If someone else has saved first, a warning is shown when you save, but if you save anyway, the whole file is overwritten with the contents saved last (see [Saving and Testing a Project](../designing/saving-and-testing.md)).
- Restart PPM before you start editing. If you open and save the project settings in a PPM that still has the old settings loaded, changes saved by others in the meantime are lost.

For things to watch out for when changing the settings during production, see [Changing the Design During Production](../designing/changing-in-production.md).

### Files in the Stuff folder

| File | When it is applied |
|---|---|
| Project files (`projects`) | When PPM starts, and when you switch projects |
| Translation files (`loc`), `defaultusersettings.ini` | When PPM starts |
| Rename scripts (`rename_scripts`) | Each time an action that uses the script is run |

## Access rights

Set the following access rights on the shared folders.

| Folder | Everyone using PPM | Project designers |
|---|---|---|
| Root folder | Read and write (including delete) | Read and write (including delete) |
| Project info folder | Read and write (including delete) | Read and write (including delete) |
| Log folder | Read and write (including delete) | Read and write (including delete) |
| Stuff folder (except the three above) | Read | Read and write |

- Actions rename, move, and delete scene files. The root folder and the project info folder need the "Modify" permission (which includes delete) in terms of Windows permissions.
- Besides the scene logs, the records of the Google Spreadsheet integration and Google Chat notifications are written to the log folder. Records of spreadsheet writes are deleted when they succeed, so the log folder also needs the delete permission (see [Google Spreadsheet Integration](../integrations/google-spreadsheet.md) and [Google Chat Notification](../integrations/google-chat.md)).
- If the root folder, the project info folder, or the log folder is specified with a relative path, it is placed inside the Stuff folder (such as `sample_data` of the sample project). In that case too, give those folders the rights in the first three rows.
- If only designers can write to the Stuff folder, staff cannot change the project settings by accident. If someone who cannot write to it tries to save the project settings, an error message is shown and nothing is saved.
- If there is no `rename_scripts` folder, PPM tries to create it and put the sample script in it. If the Stuff folder is read-only, create the `rename_scripts` folder in advance.

## Moving the shared folders

When you move the shared folders, for example when replacing the server, work in the following order.

1. Ask everyone to quit PPM.
2. Copy the Stuff folder, the root folder, the project info folder, and the log folder to the new location without changing their inner structure.
3. Rewrite `StuffFolderPath` in `conf.ini` on each PC (or the single one, if PPM itself is shared) to the new Stuff folder.
4. Start PPM, and in **Basics** of the project settings, rewrite **Project Root Folder [ROOT]**, **Project Info Folder**, and **Log Root Folder** to the new locations. Those written as relative paths can stay as they are.
5. If the old server name is written directly in **Path Macros**, condition paths of workflows, or paths of steps, rewrite them.
6. Save, and check the list and running actions with a few scenes (see [Saving and Testing a Project](../designing/saving-and-testing.md)).
7. If there are no problems, ask everyone to start PPM. It is safer to keep the files in the old location for a while.

The files in the project info folder and the log folder contain no paths.
If you move the folders as they are, they can be used as they are.

---

[← Back to contents](../index.md)
