[日本語](retakes.ja.md)

# Retakes

Asking for corrections to a scene that has gone through the workflows is called a **retake**.
When you issue a retake, the take number of the scene goes up by one, and the scene moves to a workflow group for retakes.
The mechanism is explained in [How Scenes Flow Through Workflows](../concepts/scene-flow.md).

A **rewind**, which sends a scene back to an earlier workflow within a workflow group, is issued from a row of the scene list (see [Working on a Scene](scene-operations.md)).

## Issuing a retake

1. Press the **Retake** button at the top. The **Issue New Retakes** dialog opens.
2. Enter the names of the scenes to retake in the field at the top. You can enter several scenes at once, separated by commas (`,`), colons (`:`), or semicolons (`;`).
3. Decide the **Take Number** (see "Take numbers" below).
4. In **Pass to**, press the button of the workflow group to send the scenes to.
5. In **Comment**, write what needs to be corrected.
6. Press **Retake**.

<!-- Image: images/en/using/retake-dialog.png (the Issue New Retakes dialog, showing the scene names, take number, Pass to buttons, and comment) -->

When "Retake issued." appears, it is done.
Press **Open containing folder(s)** to open the newly created scene folders.
The dialog stays open with the fields cleared, so you can issue another retake. When you are finished, press **Close**.

If PPM cannot create the scene folder or the cue file of some scenes, or cannot write to their scene info files, the scene names and the reasons are shown.
If the scene folder cannot be created or the scene info cannot be written, no retake is issued for that scene. If only the cue file cannot be created, the retake is issued, so place the cue file by hand.

### Scenes you can retake

You can issue a retake for a scene that belongs to a workflow group with "Is Retake Source" turned on in the project settings.
In the sample project, the shooting workflow group is the retake source. You can issue a retake for a scene that has reached shooting.

If you enter a scene that cannot be retaken, its name is shown in red next to the take number, grouped by reason, and the **Retake** button cannot be pressed. The reasons are:

- It is not in a retake source workflow group (the names of the retake source workflow groups are also shown).
- Its scene info could not be read.
- The take number is not larger than the current take (with **Manual**; the current take number is also shown).

If the project has no retake source workflow group or no retake workflow group, this is also shown in red, and no retake can be issued.

### Pass to

**Pass to** shows a button for each workflow group with "Is Retake Group" turned on in the project settings ("原画リテイク", "動画リテイク", and "仕上リテイク" (key animation, in-between, and ink and paint retakes) in the sample).
At first, the leftmost button is selected.

## Take numbers

In **Take Number**, choose how to decide the new take number.

- **Auto**: For each scene, the current take number plus one. A scene retaken for the first time becomes take 2.
- **Manual**: The number entered in the field on the right. Use this to match the number of a scene whose take number has gone up without PPM. Enter a number larger than the current take number.

Next to the take number, which scene becomes which take is shown in blue (e.g. `0001, 0002 -> Take2  |  0003 -> Take3`).

You can check the current take number of a scene in "Current Take" of the **scene search** (see [Main Window](main-window.md)).

## After issuing a retake

When you issue a retake, the following is done.

1. The new take number, the destination, the comment, and the date and time are recorded in the scene info file.
2. A scene folder with the take number (e.g. `c0001_t02`) is created in the destination workflow group, and `retake_start.txt` is placed in it.
3. The scene moves to the destination workflow group. The log records, for example, "Retake to Take2".
4. If the project has the scene info tag `RETAKE_WIP`, it is attached with the name of the destination workflow group as its value (e.g. "リテイク作業中:原画リテイク"). For how to remove it, see "Scene info tags used by PPM features" in [Scene Info Tags](../designing/info-tags.md).

After the retake is issued, the same process as the reload button of a workflow group is run automatically for the destination workflow group (and the workflow group the scene was in).
PPM looks for cue files such as `retake_start.txt`, and the scene is listed in the matching workflow.
Which workflow it is listed in depends on the design of the project.

In the list of a workflow group for retakes, the scene name is shown with the take number (e.g. `0001_t02`).

---

[← Back to contents](../index.md)
