[日本語](add-progress.ja.md)

# Adding Next Workflows Conditionally

The **next workflows** of an action are usually always the same.
When you want to **send only some scenes to another workflow as well**, depending on scene info tags or the existence of files, combine a conditional branch step with an **Add Next Workflow** step.

## The Add Next Workflow step

**Add Next Workflow** is a step that adds the specified workflow to the next workflows of the action.
Its only setting is the **Workflow** to add.

- When the action runs, the scene moves to the workflow specified in this step in addition to the "Next Flows".
- The confirmation dialog shows "The scene will also be sent to the workflow "...".".
- When the action is undone, the scene is also removed from the added workflow.

Use this step in "If True" or "If False" of a conditional branch step (**Conditional Branch By Info Tag**, **Conditional Branch By File Existence**).
The workflow is added only when the condition is met.

In the project settings, workflows added by this step are marked "[added by Add Next Workflow]" in the **Next Flows** list of the action.
In the flowchart, they are shown with link arrows.

### Things to know

- **The condition is judged when the confirmation dialog opens.** Changes to scene info tags or files after the confirmation dialog opens are not reflected in that action.
- **Adding a workflow alone does not list the scene in it.** Also create, with a step, a file (cue file) that matches the condition path of the added workflow.
- **A conditional branch can hold only one step.** If you want to do two or more things when the condition is met, such as adding a workflow and creating a cue file, line up as many conditional branch steps with the same condition as you need.
- **When adding a workflow of another workflow group:** Scenes are looked for only in the workflows of the workflow group they belong to (see [Progress Is Managed by Files](../concepts/progress-files.md)). To list a scene in workflows of another workflow group as well, also add a **Start Branch Flow** step along with Add Next Workflow (see [How Scenes Flow Through Workflows](../concepts/scene-flow.md)).

## Examples

### Adding a workflow by scene info tag

In this example, at the LO assistant director check, only scenes with "BG Only" are sent to a "BG layout check" workflow in addition to LO up.
Create "BG layout check" in the LO workflow group, with the condition path `[LO_CUTFOLDER]/[PREFIX][CUTNUMBER]_bg_check.txt`.

Set the steps of the action "承認（LO上がり）" (approve, LO up) of "LO 演助チェック" (LO assistant director check) as follows.

| Order | Step | Settings |
|---|---|---|
| 1 | Rename | `[LO_CUTFOLDER]/[PREFIX][CUTNUMBER][R]_LO.clip` (to LO up, as usual) |
| 2 | Conditional Branch By Info Tag | Info Name: `BG_ONLY`, Condition Value: `1`, If True: **Add Next Workflow** (Workflow: BG layout check) |
| 3 | Conditional Branch By Info Tag | Info Name: `BG_ONLY`, Condition Value: `1`, If True: **Create Cue File** (`[LO_CUTFOLDER]/[PREFIX][CUTNUMBER]_bg_check.txt`) |

Scenes with "BG Only" are listed in both LO up and BG layout check.
Scenes without it are listed only in LO up, as usual.

### Adding a workflow by file existence

You can also add a workflow only when a specific file is in the scene folder.
For example, at the key animation episode director check, only scenes with a correction instruction file (`[GENGA_CUTFOLDER]/[PREFIX][CUTNUMBER]_shusei.txt`) are sent to a "correction check" workflow as well.

In this case, use **Conditional Branch By File Existence**, put the path of the correction instruction file in **Condition Path**, and put **Add Next Workflow** in **If Exists**.
If the condition path of the "correction check" workflow is the correction instruction file itself, no step for creating a cue file is needed.

### Canceling a next workflow

Conversely, you can also **keep** scenes that meet a condition **from moving** to a next workflow.
Put **Release Progress** in a conditional branch and specify a next workflow of the action; scenes that meet the condition do not move to that workflow.
Put the step that creates the cue file of that next workflow in "If False" of a conditional branch with the same condition, so that the file is created only when the condition is not met.

---

[← Back to contents](../index.md)
