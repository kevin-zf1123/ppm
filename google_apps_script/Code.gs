/**
 * PPM - Google Apps Script endpoint
 * ------------------------------------------------------------
 * This single script serves two independent features of PPM.
 * Deploy it as a Web App and put its URL into PPM's project
 * settings ("Google Apps Script URL").
 *
 *  1. Writing progress into a Google spreadsheet.
 *     The script must be a *container-bound* script for this feature, i.e.
 *     created via "Extensions > Apps Script" from inside the spreadsheet you
 *     want PPM to write progress into. Because the script is bound to that
 *     spreadsheet, it always writes into it via
 *     SpreadsheetApp.getActiveSpreadsheet() - PPM never needs to know (or send)
 *     the spreadsheet's ID.
 *
 *  2. Posting notification messages to Google Chat.
 *     This feature does not need a spreadsheet. If you only want Chat
 *     notifications, create a standalone project at script.google.com instead
 *     of a container-bound one, and paste this code into it.
 *     The incoming webhook URLs of your Chat spaces are NOT stored in PPM;
 *     they are stored in this script's "Script properties" (see below), so that
 *     the users of PPM never see them.
 *
 * For the full setup instructions, see the PPM manual:
 *   https://github.com/opentoonz/ppm/blob/main/docs/integrations/google-spreadsheet.md
 *   https://github.com/opentoonz/ppm/blob/main/docs/integrations/google-chat.md
 *
 * このスクリプト1つで、PPMの2つの機能を提供します。ウェブアプリとしてデプロイし、
 * そのURLをPPMのプロジェクト設定（Google Apps Script URL）に入力してください。
 *
 *  1. Googleスプレッドシートへの進捗の書き込み。
 *     この機能を使うには、書き込み先のスプレッドシート内で「拡張機能 > Apps Script」から
 *     作成した「コンテナバインドスクリプト」である必要があります。スクリプトが
 *     スプレッドシートに紐づいているため、SpreadsheetApp.getActiveSpreadsheet()で常に
 *     そのスプレッドシートへ書き込みます。PPM側はスプレッドシートIDを知る必要も
 *     送信する必要もありません。
 *
 *  2. Google Chatへの通知メッセージの投稿。
 *     この機能にスプレッドシートは不要です。Chat通知だけを使う場合は、コンテナバインド
 *     ではなく、script.google.comで「新しいプロジェクト」を作成し、このコードを貼り付けて
 *     ください。ChatスペースのIncoming Webhook URLはPPMには保存せず、このスクリプトの
 *     「スクリプト プロパティ」に保存します（下記参照）。PPMの利用者には、URLは見えません。
 *
 * 手順は、PPMのマニュアルを参照してください。
 *   https://github.com/opentoonz/ppm/blob/main/docs/integrations/google-spreadsheet.ja.md
 *   https://github.com/opentoonz/ppm/blob/main/docs/integrations/google-chat.ja.md
 */

/**
 * ---- User-configurable settings / ユーザー設定 ----
 */

// [Spreadsheet] Row number where flow (step) names are written. Each name in this row
// must be unique across the row - it is looked up directly, by name.
// [スプレッドシート] フロー(工程)名が記載されている行番号。この行の各名前は行内で一意である
// 必要があります(名前そのもので直接検索されるため)。
const FLOW_NAME_ROW = 8;

// [Spreadsheet] Column letter where scene/cut numbers are written.
// [スプレッドシート] シーン(カット)番号が記載されている列のアルファベット。
const SCENE_NUMBER_COLUMN = "C";

/**
 * ---- Google Chat settings (Script properties) / Chat通知の設定（スクリプト プロパティ） ----
 *
 * Register each Chat destination in "Project Settings > Script properties" of the
 * Apps Script editor:
 *   Property: CHAT_WEBHOOK_<destination name>   Value: <the space's incoming webhook URL>
 *   e.g.      CHAT_WEBHOOK_Production           Value: https://chat.googleapis.com/v1/spaces/...
 * The destination names must match the ones registered in PPM's project settings.
 *
 * Optional properties:
 *   CHAT_DISABLED         "true" stops all Chat notifications at once (kill switch).
 *   CHAT_MAX_PER_MINUTE   Maximum number of messages per minute (default: 60).
 *
 * Apps Scriptの編集画面の「プロジェクトの設定 > スクリプト プロパティ」に、投稿先ごとに登録します。
 *   プロパティ: CHAT_WEBHOOK_<投稿先の名前>   値: <スペースのIncoming Webhook URL>
 *   例:         CHAT_WEBHOOK_Production        値: https://chat.googleapis.com/v1/spaces/...
 * 投稿先の名前は、PPMのプロジェクト設定に登録した名前と一致させてください。
 *
 * 任意のプロパティ:
 *   CHAT_DISABLED         "true"にすると、Chat通知を一括で停止します（緊急停止用）。
 *   CHAT_MAX_PER_MINUTE   1分あたりの最大投稿数（既定：60）。
 */
const CHAT_WEBHOOK_PROPERTY_PREFIX = "CHAT_WEBHOOK_";
const CHAT_DEFAULT_MAX_PER_MINUTE = 60;
const CHAT_MAX_TEXT_LENGTH = 4000; // Chat's limit for a text message is 4096 characters

/* ---------------------------------------------------------------------- */

/**
 * Run this function once from the Apps Script editor (select "authorize" in the toolbar
 * and click "Run"), then approve the permission request. The Chat notification needs the
 * permission to connect to an external service (the Chat webhook), which this script did
 * not need before. After approving, deploy a new version of the web app
 * (Deploy > Manage deployments > edit > Version: New version).
 * Without this, PPM reports "You do not have permission to call UrlFetchApp.fetch".
 *
 * Apps Scriptの編集画面で、この関数を一度だけ実行（ツールバーで「authorize」を選んで
 * 「実行」）し、権限の確認画面で承認してください。Chat通知には、外部サービス（ChatのWebhook）へ
 * 接続する権限が必要ですが、このスクリプトには、これまでその権限が不要でした。
 * 承認したあと、ウェブアプリの新しいバージョンをデプロイしてください
 * （デプロイ > デプロイを管理 > 編集 > バージョン：新バージョン）。
 * 承認していないと、PPMに「UrlFetchApp.fetchを呼び出す権限がありません」と表示されます。
 */
function authorize() {
  // A harmless request (it posts nothing). The point is that the editor asks for the permission to
  // connect to external services, or shows the error if this script is not allowed to do so.
  // 何も投稿しない、無害なリクエスト。エディタに、外部サービスへ接続する権限の確認を出させるか、
  // 許可されていない場合は、そのエラーを表示させるためのもの。
  UrlFetchApp.fetch("https://chat.googleapis.com/", { muteHttpExceptions: true });
  Logger.log("OK: this script is allowed to connect to external services.");
}

function doPost(e) {
  let requestData;
  try {
    requestData = JSON.parse(e.postData.contents);
  } catch (err) {
    return jsonOutput({ success: false, error: "Invalid request: " + err.toString() });
  }

  // Requests without "action" are spreadsheet writes (the format PPM used before Chat
  // notifications existed).
  // "action"の無いリクエストは、スプレッドシートへの書き込み（Chat通知の機能が無かった頃の形式）。
  if (requestData.action === "notifyChat")
    return handleNotifyChat(requestData);
  if (requestData.action === "listChatDestinations")
    return handleListChatDestinations();
  return handleWriteSheet(requestData);
}

function jsonOutput(object) {
  return ContentService.createTextOutput(JSON.stringify(object))
    .setMimeType(ContentService.MimeType.JSON);
}

/* ---------------------------------------------------------------------- */
/* Google Chat                                                            */
/* ---------------------------------------------------------------------- */

// Returns the names of the registered destinations (never the webhook URLs), so that PPM's
// project settings can offer them without typing the names by hand.
// 登録されている投稿先の名前の一覧を返す（Webhook URLは返さない）。PPMのプロジェクト設定で、
// 名前を手で入力せずに済ませるために使われる。
function handleListChatDestinations() {
  try {
    const names = PropertiesService.getScriptProperties().getKeys()
      .filter(function (key) {
        return key.indexOf(CHAT_WEBHOOK_PROPERTY_PREFIX) === 0 && key.length > CHAT_WEBHOOK_PROPERTY_PREFIX.length;
      })
      .map(function (key) { return key.substring(CHAT_WEBHOOK_PROPERTY_PREFIX.length); })
      .sort();
    return jsonOutput({ success: true, destinations: names });
  } catch (err) {
    return jsonOutput({ success: false, error: err.toString() });
  }
}

function handleNotifyChat(requestData) {
  try {
    const props = PropertiesService.getScriptProperties();
    if (props.getProperty("CHAT_DISABLED") === "true")
      throw new Error("Chat notifications are disabled (the script property CHAT_DISABLED is true).");

    // The webhook URL is looked up by name; the client never sends or receives it.
    // Webhook URLは名前で探す。クライアントがURLを送ったり受け取ったりすることはない。
    const destination = String(requestData.destination || "");
    const webhookUrl = destination ? props.getProperty(CHAT_WEBHOOK_PROPERTY_PREFIX + destination) : null;
    if (!webhookUrl) {
      throw new Error("Chat destination not found: \"" + destination + "\". Add the script property \""
        + CHAT_WEBHOOK_PROPERTY_PREFIX + destination + "\".");
    }

    let text = String(requestData.text == null ? "" : requestData.text);
    if (text.trim() === "")
      throw new Error("The message is empty.");
    if (text.length > CHAT_MAX_TEXT_LENGTH)
      text = text.substring(0, CHAT_MAX_TEXT_LENGTH - 1) + "…";

    // Limit the number of messages per minute, in case the endpoint URL leaks.
    // エンドポイントのURLが漏れた場合に備えて、1分あたりの投稿数を制限する。
    const limit = Number(props.getProperty("CHAT_MAX_PER_MINUTE")) || CHAT_DEFAULT_MAX_PER_MINUTE;
    const lock = LockService.getScriptLock();
    lock.waitLock(10000);
    try {
      const cache = CacheService.getScriptCache();
      const cacheKey = "chat_count_" + Math.floor(Date.now() / 60000);
      const count = Number(cache.get(cacheKey)) || 0;
      if (count >= limit) {
        return jsonOutput({
          success: false, retryable: true,
          error: "Rate limit exceeded (" + limit + " messages per minute)."
        });
      }
      cache.put(cacheKey, String(count + 1), 120);
    } finally {
      lock.releaseLock();
    }

    // Messages with the same thread key are grouped into one thread.
    // 同じスレッドキーのメッセージは、1つのスレッドにまとめられる。
    let postUrl = webhookUrl;
    const threadKey = String(requestData.threadKey || "");
    if (threadKey !== "") {
      postUrl += (postUrl.indexOf("?") >= 0 ? "&" : "?")
        + "threadKey=" + encodeURIComponent(threadKey)
        + "&messageReplyOption=REPLY_MESSAGE_FALLBACK_TO_NEW_THREAD";
    }

    const response = UrlFetchApp.fetch(postUrl, {
      method: "post",
      contentType: "application/json; charset=UTF-8",
      payload: JSON.stringify({ text: text }),
      muteHttpExceptions: true
    });
    const code = response.getResponseCode();
    if (code < 200 || code >= 300) {
      return jsonOutput({
        success: false,
        retryable: (code === 429 || code >= 500),
        error: "Google Chat returned HTTP " + code + ": " + response.getContentText().substring(0, 300)
      });
    }
    return jsonOutput({ success: true });

  } catch (err) {
    return jsonOutput({ success: false, error: err.toString() });
  }
}

/* ---------------------------------------------------------------------- */
/* Spreadsheet                                                            */
/* ---------------------------------------------------------------------- */

function handleWriteSheet(requestData) {
  // Serialize executions so that requests arriving at nearly the same time
  // (e.g. several steps of one action, or several PPM users) never run in parallel.
  // 近いタイミングで届いた複数のリクエスト(1アクション内の複数ステップや、複数のPPM
  // ユーザーからのもの)が並行実行されないよう、実行を直列化する。
  const lock = LockService.getScriptLock();
  let locked = false;
  try {
    lock.waitLock(30000);
    locked = true;

    const sheetName = requestData.sheetName;
    const cutNumbers = requestData.cutNumbers; // e.g. ["1", "2"]
    const flowName = requestData.flowName;
    const values = requestData.values;         // e.g. ["2026/08/04"]

    // This only resolves to a spreadsheet when the script is container-bound.
    // コンテナバインドスクリプトの場合のみ、紐づくスプレッドシートが取得できる
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    if (!ss) {
      throw new Error("This script is not bound to a spreadsheet. Create it via Extensions > Apps Script from inside the target spreadsheet.");
    }
    const sheet = sheetName ? ss.getSheetByName(sheetName) : ss.getSheets()[0];

    if (!sheet) {
      throw new Error("Sheet not found: " + sheetName);
    }

    const sceneNumberColIndex = sheet.getRange(SCENE_NUMBER_COLUMN + "1").getColumn();
    const lastColumn = sheet.getLastColumn();
    const lastRow = sheet.getLastRow();

    // 1. Find the column for the given flow name: search FLOW_NAME_ROW, from
    //    the column right after the scene-number column through to the last
    //    column with data.
    let targetColIndex = -1;
    const flowSearchStartCol = sceneNumberColIndex + 1;
    if (flowSearchStartCol <= lastColumn) {
      const flowNameRowValues = sheet.getRange(FLOW_NAME_ROW, flowSearchStartCol, 1, lastColumn - flowSearchStartCol + 1).getValues()[0];
      for (let i = 0; i < flowNameRowValues.length; i++) {
        if (String(flowNameRowValues[i]) === flowName) {
          targetColIndex = flowSearchStartCol + i;
          break;
        }
      }
    }
    if (targetColIndex === -1) {
      throw new Error("Flow not found: " + flowName);
    }

    // 2. Find the row for each requested cut number: search
    //    SCENE_NUMBER_COLUMN, from the row right after FLOW_NAME_ROW through
    //    to the last row with data.
    let cutRowsValues = [];
    const sceneSearchStartRow = FLOW_NAME_ROW + 1;
    if (sceneSearchStartRow <= lastRow) {
      cutRowsValues = sheet.getRange(sceneSearchStartRow, sceneNumberColIndex, lastRow - sceneSearchStartRow + 1, 1).getValues();
    }

    // 3. Update each requested cut number
    const prevValues = [];
    const notFoundCuts = []; // cut numbers that have no row in the sheet (nothing is written for them)
    const writtenCells = []; // cells that were written, for PPM's log

    for (let idx = 0; idx < cutNumbers.length; idx++) {
      let cutNum = cutNumbers[idx];
      // strip leading zeros, matching how PPM stores cut numbers in the sheet
      while (cutNum.startsWith('0') && cutNum.length > 1) {
        cutNum = cutNum.substring(1);
      }

      let foundRowIndex = -1;
      for (let r = 0; r < cutRowsValues.length; r++) {
        if (String(cutRowsValues[r][0]) === cutNum) {
          foundRowIndex = sceneSearchStartRow + r;
          break;
        }
      }

      if (foundRowIndex === -1) {
        prevValues.push("");
        notFoundCuts.push(cutNum);
        continue;
      }

      const targetCell = sheet.getRange(foundRowIndex, targetColIndex);

      // record the previous value (as displayed) for undo purposes
      const prevVal = targetCell.getDisplayValue();
      prevValues.push(prevVal);

      const newValue = (values.length === 1) ? values[0] : values[idx];
      targetCell.setValue(newValue);
      writtenCells.push({ cell: targetCell.getA1Notation(), cut: cutNum, value: newValue, cellRef: targetCell });
    }

    // Commit the writes before responding, then read each cell back so PPM's log
    // shows what the sheet actually contains.
    // 応答を返す前に書き込みを確定(flush)し、各セルを読み戻して、実際にシートに
    // 入っている値をPPM側のログで確認できるようにする。
    SpreadsheetApp.flush();
    const writtenInfo = writtenCells.map(function (w) {
      return { cell: w.cell, cut: w.cut, value: w.value, readBack: w.cellRef.getDisplayValue() };
    });

    return jsonOutput({
      success: true,
      prevValues: prevValues,
      notFoundCuts: notFoundCuts,
      written: writtenInfo
    });

  } catch (err) {
    return jsonOutput({ success: false, error: err.toString() });
  } finally {
    if (locked) lock.releaseLock();
  }
}
