[日本語](backup.ja.md)

# Backup and Recovery

All PPM information is stored as files in shared folders (see [Progress Is Managed by Files](../concepts/progress-files.md)).
The current version of PPM itself has no backup feature, so include these folders in the studio's regular backups.

## What to back up

| Target | Location | If it is lost |
|---|---|---|
| Project files | `projects` in the Stuff folder | Cannot be recreated. You have to design the workflows again. |
| Rename scripts, `defaultusersettings.ini` | Stuff folder | Cannot be recreated. |
| Scene files (including cue files) | Root folder | Cannot be recreated. |
| Workflow group membership (`LO.ini` and so on) | `progress` in the project info folder | Scenes no longer appear in the list. You have to register the scenes again. |
| Scene list of each workflow (`LO_details.ini` and so on) | `progress_cache` in the project info folder | Can be rebuilt from the cue files with the reload button of each workflow group. |
| Scene info tags and take records (`c0001.ini` and so on) | `scene_info` in the project info folder | Cannot be recreated. You have to set the scene info tags again and fix the take numbers by hand. |
| Scene logs, records of the Google integrations | Log folder | Cannot be recreated. |
| Preferences (`usersettings.ini`) | `%LOCALAPPDATA%\PPM` on each PC | Each staff member only has to set them again. |

- **Back up the root folder, the project info folder, and the log folder at the same point in time.** If you restore them from different points in time, the state of the scene files and PPM's records will not match.
- It is safest to take backups when nobody is using PPM (at night, for example).

### Before changing the project settings

Before making large changes to the project settings, copy the `.ppmprj` file in the `projects` folder.

- Keep the copy **outside** the `projects` folder, or change its extension to something other than `.ppmprj` (e.g. `stuff/projects_backup/sample_project_261030.ppmprj` or `stuff/projects/sample_project.ppmprj_bkup261030`). A `.ppmprj` file inside the `projects` folder appears in the **Project** list of the main window.
- To try new settings, you can also save the project under another ID and test it there (see [Saving and Testing a Project](../designing/saving-and-testing.md)).

## Recovery

### Restoring a project file

1. Ask everyone to quit PPM (if someone who still has PPM running saves the project settings, the restored file is overwritten).
2. Put the copied `.ppmprj` file in the `projects` folder under its original name.
3. Start PPM and check that the settings have been restored.

Restoring the settings does not restore the scene files or the scene info tags.
Check that the restored settings match the names and locations of the scene files (see [Changing the Design During Production](../designing/changing-in-production.md)).

### Rebuilding the scene list of each workflow

If the files in the `progress_cache` folder are damaged or lost, or after restoring them from a backup, press the reload button (circular arrow icon) of each workflow group in the main window.
PPM looks for the cue files again for the scenes that belong to that workflow group and rebuilds the list (see [Which Scenes Appear in the List](../using/scene-list.md)).

### Returning a single scene to an earlier state

Right after running an action, you can return it with the **Undo** button (see [Working on a Scene](../using/scene-operations.md)).
After pressing **Refresh**, or for actions run on another PC, fix the files by hand (see [Working with Files Directly](../concepts/working-with-files.md)).
You can check what was done and when in the scene log.

## Tracing what happened from the log

Each operation on a scene is added to the scene's log, one line at a time.
Read it with the **View Log** button in the main window, or open a file such as `c0001.md` in the log folder with a text editor.

```
## Log of the scene c0002

|Date and time|Workflow|Action|User name|
|-------------|----------|----------|---------|
|2026 10/29 14:34:32|Switch Workflow|Register Scene|peter|
|2026 10/29 14:34:32|LO 監督チェック|Register Scene|peter|
|2026 10/29 14:35:11|Modify Cut Info|BG Only : Register|peter|
|2026 10/30 10:12:05|LO 監督チェック|承認|paul|
```

### What is recorded

| Workflow column | Action column | Meaning |
|---|---|---|
| Workflow name | Action name | The action was run. |
| Workflow name | `[FAILED] ` + action name | The action failed partway. The steps before the failure remain done. |
| Workflow name | `[UNDO] ` + action name | The action was undone. |
| Workflow name | `[UNDO FAILED] ` + action name | Undoing failed partway. |
| Workflow name | `Rewind: Return to ...` | The scene was rewound to the workflow "..." (see [Retakes](../using/retakes.md)). |
| Workflow name | `Register Scene` | The scene was registered in that workflow with Register. |
| `Switch Workflow` | Action name, or `Register Scene` | The workflow group the scene belongs to changed. |
| `Add Branch Workflow` | Action name | The scene was added to a branch flow. |
| `Modify Cut Info` | `tag name : Register`, `tag name : Release` | A scene info tag was set or removed. |

- **User name** is the Windows user name, not the **User Name** in Preferences.
- The heading, the date format, and words such as `[FAILED]` are written in **the language of the PPM of the person who did the operation**. Lines written by a Japanese PPM are recorded like `[失敗]` or `2026年 10月30日  10時12分05秒`.
- The log records only the action name. It does not record which files were moved. Check the steps of the action in the project settings. The contents of **Show Details...** shown on failure are not recorded in the log either.

### Records of the Google integrations

Records of Google Spreadsheet writes and Google Chat notifications are kept in the following places in the log folder.

| Record | Location |
|---|---|
| Spreadsheet writes | `google_spreadsheet_cues` folder (see [Google Spreadsheet Integration](../integrations/google-spreadsheet.md)) |
| Chat notifications | `google_chat/google_chat.log` (see [Google Chat Notification](../integrations/google-chat.md)) |

---

[← Back to contents](../index.md)
