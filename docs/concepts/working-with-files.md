[日本語](working-with-files.ja.md)

# Working with Files Directly

PPM manages progress with the files of each scene, not with a central database (see [Progress Is Managed by Files](progress-files.md)).
This is so that you can fix the progress without going through PPM, by placing or renaming files by hand.

In production, things often do not go as planned.
You may want to put a scene in at a workflow in the middle, send back a scene that moved forward by mistake, or add a workflow during production.
PPM aims not at managing progress perfectly, but at getting the production finished.
So it is designed to accept such quick, practical fixes made by handling files.

However, before you handle files by hand, make sure you understand which files PPM looks at to determine the progress.
If you handle files without knowing how it works, scenes may disappear from the list or be listed in unexpected workflows.
This page explains what happens when you handle files directly, and how to do it.

## Moving cue files by hand

Which workflow a scene is in is determined by whether a cue file (the file that marks a workflow) exists.
So you can move a scene to another workflow by placing or renaming a cue file.

For example, you can fix things as follows.

- **Putting a scene in at a workflow in the middle**: Place a file in the scene folder with a name that matches the condition path of that workflow.
- **Moving a scene back or forward**: Rename the cue file so that it matches the condition path of the workflow you want to move the scene to.
- **A check workflow was added during production**: Rename the cue files of the scenes already in progress by adding the suffix of the new workflow.

When you move files by hand, note the following.

- **Cue files are only looked for in the workflows of the workflow group that the scene belongs to.**
  Even if you place a key animation file, the scene is not listed in the key animation workflows while it still belongs to LO.
  To change the workflow group, use [Registering Scenes](../using/registering-scenes.md).
- **If the name does not match a condition path, the scene is not listed anywhere.** Check the spelling of the prefix, the suffix, and the extension.
- **If there are several cue files, the scene is listed in several workflows at the same time.** If this is not intended, delete or rename the ones you do not need.
- **A mistake right after an action is easiest to undo.** Right after running an action, pressing the **Undo** button in the grayed-out row is more reliable than reverting the files by hand (see [How Scenes Flow Through Workflows](scene-flow.md)).

### After moving files by hand, press the reload button of the workflow group

PPM keeps a record of the scenes in each workflow, and when showing the list, it only checks the files of the scenes in that record.
So just moving files by hand may not make the scene appear in the new workflow.

After moving files, press the reload button (the circular arrow icon) next to the name of the workflow group that the scene belongs to in the main window.
PPM looks for the files of that workflow group again and rebuilds the scene list of each workflow.

## Editing info files and logs directly

The files in the project info folder (workflow group membership, the scene list of each workflow, and scene info) and in the log folder are text files.
You can open them in a text editor and fix them directly.
However, keep the following in mind.

- If you rewrite a workflow group membership file (such as `LO.ini`), the workflow group of scenes changes. To change the workflow group, [Registering Scenes](../using/registering-scenes.md) is easier and less error-prone.
- The scene list of each workflow (in the `progress_cache` folder) is rebuilt when you press the reload button of the workflow group. You do not need to fix it by hand.
- If you rewrite a scene info file (in the `scene_info` folder), the take number or the tags change. If the take number changes, the scene folder that PPM looks for also changes. You can set and remove tags in the **Register** dialog (see [Scene Info Tags](../using/scene-info.md)).
- A log is the record of the work. If you rewrite or delete it, you can no longer tell who did what and when.

Before editing a file directly, it is a good idea to make a copy of it.

## Do not run an action on an open file

If you run an action while the file is open in the application for your work, the rename or move may fail.
Save and close the file before running the action.
For what to do if an action fails, see [How Scenes Flow Through Workflows](scene-flow.md).

## Changing the project settings during production

If you change the project settings after production has started, the existing files may no longer match the settings.
For example:

- Changing the file naming rules in a condition path: scenes whose files have names following the old rule are no longer listed in any workflow.
- Changing a path macro or the root folder: PPM can no longer find the scene folders.
- Deleting a workflow or a workflow group: the scenes in it are no longer shown in the list.

In such cases, fix the existing files by hand to match the new settings, and press the reload button of the workflow group.
The project settings are shared by everyone in the studio, so changes should be made by the pipeline designer after checking their effects.
See [Changing the Design During Production](../designing/changing-in-production.md) for details.

---

[← Back to contents](../index.md)
