[日本語](macros.ja.md)

# Macros

A **macro** is a string in square brackets, such as `[CUTNUMBER]`, that PPM replaces with a value for each scene when it uses it.
There are **built-in macros**, which come with PPM, and **path macros**, which the project designer creates.

## Built-in macros

### Macros for paths

These can be used in the paths of path macros, condition paths of workflows, paths of steps, the scene thumbnail path, and so on.

| Macro | Replaced with | Example |
|---|---|---|
| `[ROOT]` | The root folder of the project (see [Basics](../designing/basics.md)) | `//fileserver/anime/my_title` |
| `[PREFIX]` | The scene number prefix | `c` |
| `[CUTNUMBER]` | The scene number | `0012`, `0034a`, `0056_0078` (shared scene) |
| `[SEQUENCE]` | The sequence: the leading digits of the scene number without the last two digits. Empty when "Group Scenes by 100 [SEQUENCE]" is off. | `00` (scene `0012`), `01` (scene `0123a`) |
| `[EPISODE]` | The episode name (when "Has Episodes" is on) | `01` |
| `[USER_NAME]` | The user name in Preferences (the Windows user name if empty) | `peter` |
| `[TAKE]` | The take mark. Empty for take 1, `_t02`, `_t03`, ... from take 2 | `_t02` |
| `[TAKE-1]` | The mark of the previous take. Empty for take 1, `_t01` for take 2, `_t02`, `_t03`, ... from take 3. Used as the name of the subfolder that keeps the previous take's data when a retake is issued | `_t02` (take 3) |
| `[PREV_EXISTING_TAKE]` | The mark of the newest take before the current one whose folder actually exists. Empty if none is found | `_t02` |
| `[R_TAKE]` | The take mark for shooting. Empty for take 1, `_r` for take 2, `_r02`, `_r03`, ... from take 3 | `_r` |
| `[R]` | The rewind mark. `_r` during a rewind (while the scene info tag `REWIND` is set), otherwise empty | `_r` |

- When `[SEQUENCE]` is empty, extra separators are removed so that no `//` remains in the path.
- For a shared scene (such as `0010_0011`), `[CUTNUMBER]` in the folder part is replaced with the shared scene number (`0010_0011`), and `[CUTNUMBER]` in the file name part with each individual scene number.

#### Macros for previous takes (`[TAKE-1]` and `[PREV_EXISTING_TAKE]`)

In steps that copy the data of a previous take when a retake is issued, use `[TAKE-1]` and `[PREV_EXISTING_TAKE]` for different purposes.

- **`[TAKE-1]`**: The mark of the current take number minus 1. It does not check whether any folder exists.
  Unlike `[TAKE]`, it is not empty for take 2; it becomes `_t01`.
  Use it as the name of a subfolder (`_t01`, `_t02`, ...) in the new take's scene folder, to keep the previous take's data separately for reference or as a backup.
- **`[PREV_EXISTING_TAKE]`**: Goes back through the takes before the current one, newest first, and gives the mark of the newest take whose folder in the path actually exists.
  It checks from the previous take down to take 2, and if none is found, it is empty (the take 1 folder).
  Even when retakes of different takes were sent to different workflow groups, you can copy from the folder that has the newest data.
  Because it checks whether a folder exists, use it in folder paths (it does not find file paths).

For example, "原画リテイク受付" (key animation retake reception) in the sample project has a "Copy Folder Contents" step that copies from `[GENGA_PREV_EXISTING_TAKE_CUTFOLDER]` (a path macro whose path contains `[PREV_EXISTING_TAKE]`) to `[GENGA_RETAKE_CUTFOLDER]/[TAKE-1]`.

| Situation | `[GENGA_PREV_EXISTING_TAKE_CUTFOLDER]` (from) | `[GENGA_RETAKE_CUTFOLDER]/[TAKE-1]` (to) |
|---|---|---|
| Key animation retake for take 2 | `c0001/genga` | `c0001_t02/genga/_t01` |
| Key animation retake for take 3 (take 2 was also a key animation retake) | `c0001_t02/genga` | `c0001_t03/genga/_t02` |
| Key animation retake for take 3 (take 2 was an in-between retake, so `c0001_t02/genga` does not exist) | `c0001/genga` | `c0001_t03/genga/_t02` |

#### Take macros in condition paths

In the condition paths of workflows, only the take macro chosen in **Take Notation** of the workflow group is replaced (see "Take notation" in [Workflow Groups](../designing/workflow-groups.md)).

| Take notation | Macro replaced in condition paths |
|---|---|
| None | None |
| Rewind [R] | `[R]` |
| Take [TAKE] | `[TAKE]` |
| R-Take [R_TAKE] | `[R_TAKE]` |

In the paths of steps, all take macros are replaced.

### Macros for specific places only

| Macro | Where | Replaced with |
|---|---|---|
| `[CURRENT_DATE]` | **Destination Paths** of **Copy Folder Contents**, **Folder Path** of **Create Empty Folder** | The date of execution (in the form `261030`) |
| `[CURRENT_DATE]` | **Value** of **Input Google Spreadsheet**, messages of **Notify Google Chat** | The date of execution (in the form `2026/10/30`) |
| `[USER_NAME]` | **Value** of **Input Google Spreadsheet** | The user name in Preferences |
| `[EPISODE]` | **Sheet Name** of **Input Google Spreadsheet** | The episode name |
| `[CURRENT_DATETIME]`, `[PROJECT_NAME]`, `[WORKFLOW_NAME]`, `[ACTION_NAME]` | **Message** and **Cancel Message** of **Notify Google Chat** | The date and time of execution, the project name, the workflow name, and the action name (see [Google Chat Notification](../integrations/google-chat.md)) |
| `[GENGA_OR_DOUGA]` | **XDTS File Name** of a workflow group | Whichever of `douga` and `genga` has a file (see [Workflow Groups](../designing/workflow-groups.md)) |
| `[SKIP_CUE_FILE]` | **Source Path** of **Copy Folder Contents** (written alone) | The folder of the cue file; the cue file itself is not copied (see [Steps](steps.md)) |

## Path macros

A **path macro** is a name given to the location of a frequently used folder (e.g. `[LO_CUTFOLDER]`).
Create them in **Path Macros** in the project settings (see [Path Macros](../designing/path-macros.md)).

- Built-in macros can be used in the path of a path macro (e.g. `[ROOT]/01_LO_genga/[SEQUENCE]/[PREFIX][CUTNUMBER]/LO`).
- Only one path macro can be used in one path.
- Another path macro cannot be written in the path of a path macro.
- The path of an **obfuscated** path macro is not shown on the screen, and is replaced with the real path only just before files are operated on.

The path macros of the sample project are as follows.

| Macro | Path |
|---|---|
| `[LO_CUTFOLDER]` | `[ROOT]/01_LO_genga/[SEQUENCE]/[PREFIX][CUTNUMBER]/LO` |
| `[GENGA_CUTFOLDER]` | `[ROOT]/01_LO_genga/[SEQUENCE]/[PREFIX][CUTNUMBER]/genga` |
| `[DOUGA_CUTFOLDER]` | `[ROOT]/02_douga/[SEQUENCE]/[PREFIX][CUTNUMBER]` |
| `[COLOR_CUTFOLDER]` | `[ROOT]/03_color/[SEQUENCE]/[PREFIX][CUTNUMBER]` |
| `[SHIAGE_CUTFOLDER]` | `[ROOT]/04_shiage/[SEQUENCE]/[PREFIX][CUTNUMBER]` |
| `[SATSUEI_CUTFOLDER]` | `[ROOT]/05_satsuei/[SEQUENCE]/[PREFIX][CUTNUMBER]` |
| `[GENGA_RETAKE_CUTFOLDER]` | `[ROOT]/01_LO_genga/[SEQUENCE]/[PREFIX][CUTNUMBER][TAKE]/genga` |
| `[DOUGA_RETAKE_CUTFOLDER]` | `[ROOT]/02_douga/[SEQUENCE]/[PREFIX][CUTNUMBER][TAKE]` |
| `[SHIAGE_RETAKE_CUTFOLDER]` | `[ROOT]/04_shiage/[SEQUENCE]/[PREFIX][CUTNUMBER][TAKE]` |

---

[← Back to contents](../index.md)
