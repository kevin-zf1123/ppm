[日本語](progress-files.ja.md)

# Progress Is Managed by Files

PPM has no dedicated server or database for recording progress.
Progress is represented by the following files in the shared folder.

| What it represents | Which file | Location |
|---|---|---|
| Which workflow a scene is in | Cue files (the work files of the scene, or marker files) | The scene folder of each scene |
| Which workflow group a scene belongs to | A list of scenes for each workflow group | The project info folder |
| Tags and takes of a scene | A scene info file for each scene | The project info folder |
| Who did what, and when | A log for each scene | The log folder |

While running, PPM reads these files to build the list, and rewrites them when you run an action.
Even if PPM is used on several PCs, everyone sees the same files in the shared folder, so the same progress is shown.

## Cue files

Which workflow a scene is in is determined by whether a **cue file** exists.
For each scene, PPM looks for files that match the **condition path** of each workflow (see [Glossary](glossary.md)).

For example, in the sample project, if the LO scene folder of scene `c0001` contains

- `c0001_LO_ej_s_k_e.clip`, the scene is in the LO episode director check;
- `c0001_LO_ej_s_k.clip`, the scene is in the LO director check;
- `c0001_LO.clip`, the scene is in LO done.

So even without PPM, you can see the progress by looking at the file names in Explorer.

However, cue files are only looked for in the **workflows of the workflow group that the scene belongs to**.
Even if a key animation file is placed in the folder of a scene that belongs to LO, the scene is not listed in the key animation workflows.
The conditions for a scene to be listed are explained in detail in [Which Scenes Appear in the List](../using/scene-list.md).

## Workflow group membership and the scene list of each workflow

Which workflow group a scene belongs to is recorded in a file for each workflow group (`LO.ini`, `genga.ini`, and so on) in the `progress` folder of the project info folder.
These files are rewritten when you register a scene, or when you run an action that moves a scene to another workflow group, such as "原画に移動" (move to key animation).

PPM also stores a list of the scenes in each workflow in the `progress_cache` folder of the project info folder, as a cache to make searches faster.
When showing the list, PPM checks the cue files only for the scenes in this list.
This is because looking for files for every workflow of every scene would take a long time.

This list is updated when you run an action.
Also, if you press the reload button (the circular arrow icon) next to the name of a workflow group in the main window, PPM looks for the files of the scenes in that workflow group again and rebuilds the list.
Press this button after you have moved files by hand (see [Working with Files Directly](working-with-files.md)).

## Scene info

The tags attached to a scene ([scene info tags](glossary.md)) and the record of its takes are stored in a settings file for each scene.
The file name is the prefix and the scene number, as in `c0001.ini`.

## Logs

The operations performed on a scene are appended to the log file of the scene.
The file name is the prefix and the scene number with the `.md` extension, as in `c0001.md`.

```
## Log of the scene c0001

|Date and time|Workflow|Action|User name|
|-------------|----------|----------|---------|
|2026 10/27 17:34:31|LO 演出チェック|承認して監督に回す|peter|
```

The log records the date and time, the workflow, the action, and the Windows user name.
The log is a Markdown table, so you can read it as it is in a text editor.
You can also view it with the **View Log** button in the PPM list.

## Where the files are stored

In the project settings, you specify the following three folders used by PPM.

| Folder | Contents |
|---|---|
| Root folder (`[ROOT]`) | The folder for the scene files. Scene folders are created in it, at the locations defined by the path macros |
| Project info folder | Workflow group membership, the scene list of each workflow, and scene info |
| Log folder | The log of each scene |

In the sample project, they are `sample_data`, `sample_data/PPM/info`, and `sample_data/PPM/log` in the Stuff folder.
The project info folder and the log folder have the following structure.

```
PPM/
├─ info/                      … Project info folder
│  ├─ progress/               … Workflow group membership
│  │  ├─ LO.ini               … Scenes belonging to the workflow group "LO"
│  │  └─ genga.ini            … Scenes belonging to the workflow group "原画" (key animation)
│  ├─ progress_cache/         … Scene list of each workflow (a cache rebuilt by the reload button)
│  │  ├─ LO_details.ini
│  │  └─ genga_details.ini
│  └─ scene_info/             … Scene info
│     └─ 00/                  … A folder for each sequence
│        └─ c0001.ini
└─ log/                       … Log folder
   └─ 00/                     … A folder for each sequence
      └─ c0001.md
```

- In a project that uses **sequences**, scene info and logs are stored in a folder for each sequence (`00`, `01`, ...). In a project that does not, they are stored directly in the `scene_info` folder or the log folder.
- In a project that uses **episodes**, a folder for each episode is created in the project info folder and the log folder, and each of them has the structure above.

Put all these folders in the shared folder so that everyone who uses PPM can read and write them.
See [Running PPM on a Shared Folder](../administration/shared-operation.md) for details.

---

[← Back to contents](../index.md)
