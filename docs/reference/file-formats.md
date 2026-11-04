[日本語](file-formats.ja.md)

# File Formats

This page describes the formats of the files PPM reads and writes.
For where each file is placed, see [Progress Is Managed by Files](../concepts/progress-files.md).

The files on this page can be opened and read in a text editor.
However, edit them by hand only when there is no other way (see [Working with Files Directly](../concepts/working-with-files.md)).
When you edit them, do it while nobody is using PPM, and save them in UTF-8.

| File | Location | Format |
|---|---|---|
| Project file (`.ppmprj`) | `projects` in the Stuff folder | JSON |
| Cue files | In the root folder (where the condition paths of the workflows say) | No rules |
| Workflow group membership (`LO.ini` and so on) | `progress` in the project info folder | INI |
| Scene list of each workflow (`LO_details.ini` and so on) | `progress_cache` in the project info folder | INI |
| Scene info tags (`c0001.ini` and so on) | `scene_info` in the project info folder | INI |
| Logs (`c0001.md` and so on) | Log folder | Markdown |

## Project file (.ppmprj)

A JSON file that holds all the project settings.
The file name (without the extension) is the project ID.
Saving in the project settings dialog rewrites the whole file.

The main keys are as follows.

| Key | Contents | Item in the project settings |
|---|---|---|
| `title`, `description` | The project name and description | Basics |
| `projectRootPath`, `projectInfoFolderPath`, `logRootPath` | The project root folder, project info folder, and log root folder | Basics |
| `cutNumberPrefix` | The scene number prefix | Basics |
| `useSequence` | Whether to group scenes by 100 | Basics |
| `hasEpisodes`, `episodeNames` | Whether episodes are used, and the episode names | Basics |
| `sceneThumbnailPath` | The scene thumbnail path | Basics |
| `gasUrl`, `gasUrlObfuscated` | The Google Apps Script URL (obfuscated) | Basics |
| `googleChatDestinations` | The names of the Google Chat destinations | Basics |
| `pathMacros` | Path macros (`macroStr`, `path`, `labelName`; obfuscated ones have `obfuscated`, and their `path` is encrypted) | Path Macros |
| `flowGroupInfo` | Workflow groups (`flowGroupId`, `idName`, `labelName`, `standardCutFolderMacro`, `takeNotationStyle`, `xdtsFileName`, and so on) | Workflow Groups |
| `sceneInfoTags` | Scene info tags (`idName`, `type`, `canManuallySet`, display settings) | Info Tags |
| `workflows` | Workflows (see below) | Workflows - List, Workflows - Flowchart |
| `roles`, `flowChartLayout` | The role buttons and the flowchart layout | Workflows - Flowchart |

`workflows` is nested as follows.

```json
{
  "WorkflowName": "LO 演出チェック",
  "WorkflowTypeId": 0,
  "WorkflowGroupIdName": "LO",
  "Suffix": "_e",
  "ClearsRewindOnComplete": false,
  "Protocols": [
    {
      "MediaType": "Digital",
      "ConditionPath": "[LO_CUTFOLDER]/[PREFIX][CUTNUMBER][R]_LO_ej_s_k_e.clip",
      "Actions": [
        {
          "ActionName": "承認して監督に回す",
          "NextFlowTypeIds": [1],
          "Steps": [
            {
              "ActionStepType": "Step_Rename",
              "ActionStepVariables": {
                "RenameTo": ["[LO_CUTFOLDER]/[PREFIX][CUTNUMBER][R]_LO_ej_s_k.clip"]
              }
            }
          ]
        }
      ]
    }
  ]
}
```

- Workflows are identified by `WorkflowTypeId` (the workflow number). `NextFlowTypeIds` is the list of the numbers of the next workflows.
- The workflow group a workflow belongs to is given by `WorkflowGroupIdName` (the ID name of the workflow group).
- `ActionStepType` of a step is the name of the step type, starting with `Step_`. The contents of `ActionStepVariables` differ for each step type.

> The example above omits some parts for clarity.

## Cue files

A cue file is a file that matches the condition path of a workflow.
Whether a scene is in a workflow is decided only by whether the file exists. The contents of the file do not matter.

- A working file (`.clip`, `.tvpp`, and so on) can itself be the cue file.
- A file used only as a marker can be an empty text file. The **Create Cue File** step can write text, such as instructions for the artist, into it (see [Steps](steps.md)).

## Workflow group membership

The `progress` folder has a file named `<ID name of the workflow group>.ini` for each workflow group, which lists the numbers of the scenes that belong to the workflow group.

```ini
[General]
cutList=0001, 0002
cutList_branch=0003
```

| Key | Contents |
|---|---|
| `cutList` | Scenes that belong to this workflow group (main flow) |
| `cutList_branch` | Scenes that are also in this workflow group as a branch flow |

## Scene list of each workflow

`<ID name of the workflow group>_details.ini` in the `progress_cache` folder lists, for each workflow, the numbers of the scenes that are in that workflow now.

```ini
[0]
name=LO 演出チェック
cutList=0001

[1]
name=LO 監督チェック
cutList=@Invalid()
```

- The group names (`[0]` and so on) are the workflow numbers (`WorkflowTypeId`). `name` is the workflow name, written for readability.
- `@Invalid()` means there are no scenes.
- This file can be rebuilt from the cue files with the reload button (circular arrow icon) of the workflow group (see [Backup and Recovery](../administration/backup.md)).
- While it is being rewritten, a `..._details.lock` file is created temporarily in the same folder (see [Running PPM on a Shared Folder](../administration/shared-operation.md)).

## Scene info tags

The `scene_info` folder has a file `<prefix><scene number>.ini` (e.g. `c0001.ini`) for each scene.
In projects that use sequences, the files are placed in a folder for each sequence (`00`, `01`, ...).

```ini
[General]
BG_ONLY=1
REWIND=WIP

[RetakeInfo]
CurrentTake=2
t02\DateTime=Fri Oct 30 10:12:05 2026
t02\PassedTo=genga_retake
t02\Comment=Fix the lip sync
```

| Group | Key | Contents |
|---|---|---|
| `General` | ID name of the scene info tag | The value of the scene info tag: `1` for Bool, the value for Enum, the string itself for String. |
| `RetakeInfo` | `CurrentTake` | The current take number. It increases when a retake is issued. |
| `RetakeInfo` | `t02\DateTime`, `t02\PassedTo`, `t02\Comment`, and so on | The date and time the retake for that take was issued, the ID name of the workflow group it was passed to, and the comment. |

The take number is used by macros such as `[TAKE]`.
Be careful when editing `CurrentTake` by hand, because it can change where the scene folder is.

## Logs

The log folder has a file `<prefix><scene number>.md` (e.g. `c0001.md`) for each scene.
In projects that use sequences, the files are placed in a folder for each sequence.

```
## Log of the scene c0001

|Date and time|Workflow|Action|User name|
|-------------|----------|----------|---------|
|2026 10/27 17:34:31|LO 演出チェック|承認して監督に回す|peter|
```

- It is a Markdown table, with one line per operation. New lines are added at the end of the file.
- For what is recorded, see "Tracing what happened from the log" in [Backup and Recovery](../administration/backup.md).

## Other files

| File | Description |
|---|---|
| `conf.ini` | The location of the Stuff folder (see [The Stuff Folder](../administration/stuff-folder.md)) |
| `defaultusersettings.ini` | The initial values of Preferences (see [The Stuff Folder](../administration/stuff-folder.md)) |
| `usersettings.ini` | The Preferences of each PC (see [Preferences](../using/preferences.md)) |
| Files in the `google_spreadsheet_cues` folder | Records of Google Spreadsheet writes (see [Google Spreadsheet Integration](../integrations/google-spreadsheet.md)) |
| `google_chat/google_chat.log` | Records of Google Chat notifications (see [Google Chat Notification](../integrations/google-chat.md)) |
| Rename scripts (`.js`) | See [Rename Scripts](../integrations/rename-scripts.md) |
| Translation files (`.qm`) | See [UI Language and Translation Files](../administration/translations.md) |

---

[← Back to contents](../index.md)
