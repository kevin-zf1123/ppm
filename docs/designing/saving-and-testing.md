[日本語](saving-and-testing.ja.md)

# Saving and Testing a Project

## Saving the project

Press **Save** at the bottom left of the Project Settings dialog to save the settings to the project file.

- Pressing **Save** does not close the dialog. You can continue editing.
- While there are unsaved changes, `*` is shown at the end of the dialog title.
- If you press **Close** while there are unsaved changes, a message asks whether to save them or discard them.
- The project cannot be saved while the **Project ID** field at the bottom left is empty. If you try to save, a message asks you to enter the project ID.
- The **Project ID** is the file name of the project. It is different from the **Project Title** in the Basics (the name shown on the screen).
- If PPM cannot write to the project file (for example, the Stuff folder is read-only), an error message is shown. In this case, the `*` in the title stays, and the changes are still treated as unsaved.

### Where the project is saved

The project is saved as `project ID.ppmprj` in the `projects` folder of the Stuff folder.

```
Stuff/
└─ projects/
   ├─ sample_project.ppmprj
   └─ my_title.ppmprj
```

If you change the project ID field and save, the project is saved as another project.
The file of the original project stays as it is (see "Creating a new project" in [Design Process and Preparation](overview.md)).
If a project with that ID already exists, a message asks whether to overwrite it.

### After saving

When you save and close the dialog, the main window reloads the saved project.
At this time, for every workflow group, the same process as the reload button of the workflow group is performed (looking for the scene files again and rebuilding the list of scenes in each workflow).

The change is not reflected immediately in PPM on other PCs.
On other PCs, the new settings are loaded when PPM is restarted, or when another project is selected in the main window and then the project is selected again.

> **Note**: The whole project file is overwritten with the contents saved last.
> If someone else has saved it first, a message is shown, as described in "Confirmation when saving" below. If you overwrite it anyway, that person's changes are lost.
> We recommend deciding on one person who edits the settings.

### Confirmation when saving

If you change "Group Scenes by 100 [SEQUENCE]" and try to save, a confirmation message is shown.
If some settings (path macros, scene thumbnail path, workflow groups, workflows) still use `[SEQUENCE]`, they are listed as well.
For the cautions when changing this during production, see [Changing the Design During Production](changing-in-production.md).

A warning message is also shown when you try to save if someone else has saved the project file after this PPM loaded the project.

- Press **Save** to overwrite the file with the contents of this PPM. The changes of the other person are lost.
- To keep the changes of the other person, press **Cancel** and close the Project Settings without saving. Then reload the project by restarting PPM, or by selecting another project in the main window and then selecting the project again, and edit it again.

### Backup

Before making large changes to the settings, it is a good idea to copy the `.ppmprj` file in the `projects` folder.
To go back, restore the copied file to its original name.
See [Backup and Recovery](../administration/backup.md) for details.

## Checks before production

Before using a project with the production folders, run it through with test folders.

### 1. Prepare a test project

1. Open the project you created, change the project ID to something like `my_title_test`, and save.
2. In **Basics**, change the root folder, the project info folder, and the log folder to test folders, and save.

In the test folder, prepare some scene folders with the same folder structure as production.

### 2. Check for missing settings

In **Workflows - Flowchart**, check that no workflow or link has a warning icon (see [Designing Workflows](workflows.md)).
If a worker is meant to put the file by hand, a warning is not a problem.

### 3. Run a test scene through the workflows

1. In the main window, right-click the first workflow and register a test scene with **Register a scene starting from this workflow** (see [Registering Scenes](../using/registering-scenes.md)).
2. Run the action of each workflow in order. In the confirmation dialog, check that the rename and copy destinations are as intended.
3. Press **Refresh** and check that the scene is listed in the next workflow.
4. Use **Open Containing Folder** to check the names and locations of the files in Explorer as well.
5. Press **Undo** and check that everything goes back.
6. Use **View Log** to check that the log is recorded.

If you use actions that cross workflow groups, actions with conditional branches, retakes, or rewinds, try them as well.
If you use the Google Spreadsheet integration, try it with a test spreadsheet.

If a scene is not listed in the workflow you expected, see how to check in [Which Scenes Appear in the List](../using/scene-list.md).

### 4. Switch to production

When the test is done, check the folder settings of the production project, and let the staff start using it.
If you delete the test project file from the `projects` folder, staff will not choose it by mistake.

---

[← Back to contents](../index.md)
