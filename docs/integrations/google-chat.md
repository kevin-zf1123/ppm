[日本語](google-chat.ja.md)

# Google Chat Notification

When an action runs, PPM can send a notification to a Google Chat space.
For example, when a scene reaches "key animation up", PPM can let the space of the in-between staff know.

Notifications are sent through a Google Apps Script (GAS) web app.
The webhook URLs of the spaces are not saved in PPM but in the script.
Therefore, the staff using PPM never see the webhook URLs.

> The names and layout of Google's screens may change. If you cannot find something, also see Google's help.

## Creating a webhook in a Chat space

Create a webhook for each space you want to notify.

1. In Google Chat, open the space you want to notify.
2. Click the name of the space and choose **Apps & integrations**.
3. Choose **Add webhooks**, enter a name (e.g. `PPM`), and save.
4. Copy the URL of the created webhook.

Webhooks can be used with Google Workspace accounts.
If webhooks are not allowed by your organization's settings, ask your Google Workspace administrator.

## Preparing the Google Apps Script

Use the same script as the Google Spreadsheet integration, `google_apps_script/Code.gs`.

- **If you also use the spreadsheet integration**: Add the Chat settings to the script you created in [Google Spreadsheet Integration](google-spreadsheet.md).
- **If you only use Chat notifications**: Create a **New project** at [script.google.com](https://script.google.com/), paste the contents of `Code.gs`, and save.

### Registering destinations

1. In the Apps Script editor, open **Project Settings** (gear icon) on the left.
2. Under **Script Properties**, add the following property for each destination.

| Property | Value |
|---|---|
| `CHAT_WEBHOOK_destination name` (e.g. `CHAT_WEBHOOK_douga`) | The webhook URL of the space |

The **destination name** (the part after `CHAT_WEBHOOK_`) is the name used to choose this destination in PPM.

If needed, you can also add the following properties.

| Property | Contents |
|---|---|
| `CHAT_DISABLED` | When `true`, all Chat notifications are stopped (for emergencies). |
| `CHAT_MAX_PER_MINUTE` | The maximum number of notifications per minute (default `60`). |

### Authorizing external connections

Posting to Chat needs permission to connect to an external service.

1. At the top of the editor, choose `authorize` as the function to run, and press **Run**.
2. When the permission screen is shown, approve it.

Without this, sending a notification fails with an error saying that you do not have permission to call UrlFetchApp.fetch.

### Deploying

Deploy the script as a web app.
The procedure is the same as "Deploying the Apps Script web app" in [Google Spreadsheet Integration](google-spreadsheet.md).
If you added the Chat settings to a script that is already deployed, deploy it again as a **New version** in **Manage deployments**.

## Registering destinations in PPM

1. Open **Basics** in **Project Settings**.
2. Enter the web app URL in **Google Apps Script URL** (if it is already set for the spreadsheet integration, leave it as it is).
3. In **Google Chat Destinations**, enter the destination names registered in the script properties, separated by commas or semicolons.
   Press **Get from Script** to load the names registered in the script and add the ones that are not entered yet.
4. Press **Send Test Message** and check that the test message arrives. If there are several destinations, choose the one to send to from the menu.
5. Press **Save**.

When you press **Get from Script**, names entered in PPM that are not registered in the script are also shown (notifications to them will fail).
If you rename only one destination, the steps that use it are also changed to the new name.

## Adding a step to an action

Add a **Notify Google Chat** step to the action that should send a notification (see [Designing Workflows](../designing/workflows.md)).

| Item | Contents |
|---|---|
| **Destination** | Choose from the destinations registered in Basics. |
| **Message** | The message to send. If empty, the default message shown in gray (project, scene, date and time, workflow, action, user) is sent. |
| **Allow Free Text** | When on, a field for adding to the message is shown in the action confirmation dialog. What is written there is added after the message. |
| **Notify On Cancel** | When on, a cancel notification is sent when the action is undone. |
| **Cancel Message (optional)** | The message sent when the action is undone. If empty, the original message is sent again after the line "[The following operation was canceled]". |
| **Thread Key (optional)** | Messages with the same thread key are grouped into one thread in Google Chat. For example, with `[CUTNUMBER]`, the notifications and cancel notifications of a scene line up in the same thread. |

The following macros can be used in the message, cancel message, and thread key.

| Macro | Replaced with |
|---|---|
| `[CUTNUMBER]`, `[SEQUENCE]`, `[EPISODE]` | The scene number, sequence, and episode |
| `[USER_NAME]` | The user name in Preferences |
| `[CURRENT_DATE]` | The date of execution (in the form `2026/10/30`) |
| `[CURRENT_DATETIME]` | The date and time of execution |
| `[PROJECT_NAME]` | The project title in Basics |
| `[WORKFLOW_NAME]` | The workflow name |
| `[ACTION_NAME]` | The action name |

## What happens when a notification is sent

- The action confirmation dialog shows the message to be sent.
- The notification is sent in the background after you press **OK**. Consecutive notifications are sent about one second apart, so as not to exceed Google Chat's posting limit.
- On temporary errors (such as when Google is busy or the connection fails), PPM waits a little and tries again, up to 3 times.
- If the notification cannot be sent, a warning is shown.
- If you try to quit PPM before sending is done, a confirmation message is shown (see [Main Window](../using/main-window.md)).
- This step is skipped when **Google Apps Script URL** is empty, or when the destination is not registered in Basics.

A record of sent notifications is kept in `google_chat/google_chat.log` in the log folder (destination name, success or failure, and the reason for failure).
The URL of the script is not recorded.

---

[← Back to contents](../index.md)
