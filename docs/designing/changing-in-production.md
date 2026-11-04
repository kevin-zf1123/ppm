[日本語](changing-in-production.ja.md)

# Changing the Design During Production

The design of a project can be changed even after production has started.
Adding a check workflow or fixing file naming rules in the middle of production is common.

PPM progress is represented by the scene files and some record files (see [Progress Is Managed by Files](../concepts/progress-files.md)).
What you need to watch when you change the design is that **existing files and records may no longer match the new design**.
Files that no longer match can be used again once you fix them by hand (see [Working with Files Directly](../concepts/working-with-files.md)).

When you save the project, the scene files are looked for again and the list of scenes in each workflow is rebuilt for every workflow group (see [Saving and Testing a Project](saving-and-testing.md)).
Therefore, **anything that can be decided from cue files** is rearranged according to the new design when you save.

## Changes that are safe

Changing the following settings during production does not affect existing records.

- Project title and description
- Workflow names, suffixes, and colors
- Label names of workflow groups
- Action names, the steps of actions, and next workflows (used from the next time the action runs)
- Labels and colors of scene info tags, and whether they can be set manually
- Roles
- Scene thumbnail path, XDTS file names, and subfolders for the Open button
- Google Apps Script URL and Google Chat destinations
- The order of workflows

**Adding workflows** is also fine.
When you save, scenes that have a file matching the condition path of the new workflow are listed in it.
If you add a workflow in the middle of a flow, scenes that are already in later workflows simply continue.
To put a current scene into the new workflow, rename its cue file to match the new condition path (see the examples in [Working with Files Directly](../concepts/working-with-files.md)).

## Changes that need care

### Condition paths

If you change a condition path, files with the old names no longer match the condition of that workflow.
After saving, the scenes are no longer listed in that workflow.
To list them again, rename the existing files to match the new condition path, and press the reload button (circular arrow icon) of the workflow group.

### Deleting workflows

If you delete a workflow, the scenes that were in it are no longer listed in any workflow.
The scene files are not deleted, so if you rename them to match the condition path of another workflow, they are listed there.

Even if you recreate a deleted workflow with the same name, the workflow gets a new ID.
The checked and locked states of workflows in the main window are recorded by ID, so you need to check and lock the recreated workflow again.

### ID names of workflow groups

The ID name of a workflow group is used in the file names of the workflow group membership record (such as `progress/LO.ini`) and the list of scenes in each workflow (such as `progress_cache/LO_details.ini`).

If you change the ID name, the existing membership records are no longer found, and the scenes of that workflow group are no longer listed.
If you must change it, rename `progress/old ID name.ini` and `progress_cache/old ID name_details.ini` in the project info folder to match the new ID name.
(The destination of "Move Flow Group" steps follows the change of the ID name automatically.)

### IDs of workflow groups

The ID of a workflow group is used for the order of workflow groups and for recording whether the header is expanded in the list view of the main window.
If you change the ID, the workflows of that workflow group and the workflow groups specified in steps such as "Move Flow Group" follow the new ID automatically. The records of scenes are not affected either.
However, on each PC, whether the header of that workflow group is expanded is reset.

### ID names of scene info tags

Scene info tags are recorded in the file of each scene by ID name.
If you change the ID name, the tags attached so far are no longer shown.
You can restore them by rewriting the name by hand in the scene info files (such as `c0001.ini` in the `scene_info` folder).

Do not change `REWIND` and `BANK`, because PPM features use these names (see [Scene Info Tags](info-tags.md)).

### Scene number prefix

The prefix is also used in the names of the scene info files (`c0001.ini`) and log files (`c0001.md`).
If you change it, the existing scene info (including the take records) and logs are no longer found.
If you change it, rename these files as well.

### Group scenes by 100 [SEQUENCE]

Turning this on or off changes the folder structure in which scene info and logs are looked for (whether sequence folders are used).
PPM does not reorganize the existing folders.
The existing scene info (including the take records) and logs are no longer found, and `[TAKE]` and `[R_TAKE]` may point to the wrong folders.

We recommend not changing this during production.
If you change it, move the files in the log folder and in the `scene_info` folder of the project info folder by hand to match the new structure.
Also rewrite `[SEQUENCE]` in paths as needed.

### Folder locations

When you change the root folder, the project info folder, the log folder, or the paths of path macros, PPM does not move any files.
Move the existing files to the new location before saving.
For procedures such as moving the shared folder to another server, see [Running PPM on a Shared Folder](../administration/shared-operation.md).

### Episodes

When you turn on "Has Episodes", a folder for each episode is used in the project info folder and the log folder.
The existing records are outside the episode folders, so they are no longer found.
For a title with episodes, turn on "Has Episodes" before production starts.
Adding episode names later is no problem.

---

[← Back to contents](../index.md)
