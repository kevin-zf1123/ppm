[English](file-formats.md)

# ファイル形式

PPM が読み書きするファイルの形式をまとめます。
どのファイルがどこに置かれるかは、[進捗はファイルで管理される](../concepts/progress-files.ja.md) を参照してください。

ここに書いたファイルは、テキストエディターで開いて読めます。
ただし、手で書き換えるのは、ほかに方法がないときだけにしてください（[ファイルを直接扱う](../concepts/working-with-files.ja.md) を参照）。
書き換えるときは、PPM を使っている人がいないときに、文字コードを UTF-8 のまま保存します。

| ファイル | 場所 | 形式 |
|---|---|---|
| プロジェクトファイル（`.ppmprj`） | Stuff フォルダの `projects` | JSON |
| キューファイル | ルートフォルダの中（工程の条件パスで決めた場所） | 決まりはありません |
| 工程グループの所属（`LO.ini` など） | プロジェクト情報フォルダの `progress` | INI |
| 工程ごとのカットの一覧（`LO_details.ini` など） | プロジェクト情報フォルダの `progress_cache` | INI |
| カット情報（`c0001.ini` など） | プロジェクト情報フォルダの `scene_info` | INI |
| ログ（`c0001.md` など） | ログフォルダ | Markdown |

## プロジェクトファイル（.ppmprj）

プロジェクトの設定をすべて保存した JSON ファイルです。
ファイル名（拡張子を除く）が、プロジェクトIDになります。
プロジェクト設定のダイアログで保存すると、ファイル全体が書き直されます。

主なキーは、次のとおりです。

| キー | 内容 | プロジェクト設定の項目 |
|---|---|---|
| `title`、`description` | プロジェクト名と説明 | 基本情報 |
| `projectRootPath`、`projectInfoFolderPath`、`logRootPath` | ルートフォルダ、進捗情報用フォルダ、履歴のルートフォルダ | 基本情報 |
| `cutNumberPrefix` | カット番号の接頭辞 | 基本情報 |
| `useSequence` | 100カットごとにまとめるかどうか | 基本情報 |
| `hasEpisodes`、`episodeNames` | 話数を使うかどうかと、話数の名前 | 基本情報 |
| `sceneThumbnailPath` | カットサムネイルのパス | 基本情報 |
| `gasUrl`、`gasUrlObfuscated` | Google Apps Script の URL（難読化されています） | 基本情報 |
| `googleChatDestinations` | Google Chat の投稿先の名前 | 基本情報 |
| `pathMacros` | パスマクロ（`macroStr`、`path`、`labelName`。難読化したものは `obfuscated` が付き、`path` が暗号化されます） | パスマクロ |
| `flowGroupInfo` | 工程グループ（`flowGroupId`、`idName`、`labelName`、`standardCutFolderMacro`、`takeNotationStyle`、`xdtsFileName` など） | 工程グループ |
| `sceneInfoTags` | カット情報（`idName`、`type`、`canManuallySet`、表示の設定） | カット情報 |
| `workflows` | 工程（下を参照） | 工程 - 一覧表示、工程 - フローチャート表示 |
| `roles`、`flowChartLayout` | 役割ボタンと、フローチャートの配置 | 工程 - フローチャート表示 |

`workflows` の中は、次のような入れ子になっています。

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

- 工程は `WorkflowTypeId`（工程の番号）で区別されます。`NextFlowTypeIds` は、次の工程の番号の一覧です。
- 工程が属する工程グループは、`WorkflowGroupIdName`（工程グループの ID名）で指定されます。
- ステップの `ActionStepType` は、`Step_` で始まるステップの種類の名前です。`ActionStepVariables` の中身は、ステップの種類ごとに違います。

> 上の例は、説明のために一部を省いています。

## キューファイル

キューファイルは、工程の条件パスに当てはまるファイルです。
カットがその工程にいるかどうかは、ファイルがあるかどうかだけで決まります。ファイルの中身は関係ありません。

- 作業用のファイル（`.clip`、`.tvpp` など）を、そのままキューファイルにできます。
- 目印のためだけのファイルなら、空のテキストファイルでかまいません。**キューファイルを作成** のステップでは、作業者への説明などの文章を書き込めます（[ステップ一覧](steps.ja.md) を参照）。

## 工程グループの所属

`progress` フォルダに、工程グループごとの `工程グループのID名.ini` というファイルがあり、その工程グループに所属しているカットの番号が並んでいます。

```ini
[General]
cutList=0001, 0002
cutList_branch=0003
```

| キー | 内容 |
|---|---|
| `cutList` | この工程グループに所属している（主流の）カット |
| `cutList_branch` | 傍流として、この工程グループにも加わっているカット |

## 工程ごとのカットの一覧

`progress_cache` フォルダの `工程グループのID名_details.ini` に、工程ごとに、今その工程にいるカットの番号が並んでいます。

```ini
[0]
name=LO 演出チェック
cutList=0001

[1]
name=LO 監督チェック
cutList=@Invalid()
```

- グループの名前（`[0]` など）は、工程の番号（`WorkflowTypeId`）です。`name` は、読みやすくするために書かれている工程の名前です。
- `@Invalid()` は、カットがないことを表します。
- このファイルは、工程グループの更新ボタン（回転矢印のアイコン）で、キューファイルから作り直せます（[バックアップと復旧](../administration/backup.ja.md) を参照）。
- 書き換えている間は、同じフォルダに `〜_details.lock` が一時的に作られます（[共有フォルダでの運用](../administration/shared-operation.ja.md) を参照）。

## カット情報

`scene_info` フォルダに、カットごとに `接頭辞＋カット番号.ini`（例：`c0001.ini`）があります。
シークエンスを使うプロジェクトでは、シークエンスごとのフォルダ（`00`、`01`…）の中に置かれます。

```ini
[General]
BG_ONLY=1
REWIND=WIP

[RetakeInfo]
CurrentTake=2
t02\DateTime=Fri Oct 30 10:12:05 2026
t02\PassedTo=genga_retake
t02\Comment=口パクの修正
```

| グループ | キー | 内容 |
|---|---|---|
| `General` | カット情報の ID名 | カット情報の値です。真偽値は `1`、列挙型は値、文字列はその文字列です。 |
| `RetakeInfo` | `CurrentTake` | 今のテイクの番号です。リテイクを出すと増えます。 |
| `RetakeInfo` | `t02\DateTime`、`t02\PassedTo`、`t02\Comment` など | そのテイクのリテイクを出した日時、渡した先の工程グループの ID名、コメントです。 |

テイクの番号は、`[TAKE]` などのマクロに使われます。
`CurrentTake` を手で書き換えると、カットフォルダの場所が変わってしまうことがあるので、注意してください。

## ログ

ログフォルダに、カットごとに `接頭辞＋カット番号.md`（例：`c0001.md`）があります。
シークエンスを使うプロジェクトでは、シークエンスごとのフォルダの中に置かれます。

```
## カット c0001 のログ

|日時|Workflow|アクション|ユーザ名|
|-------------|----------|----------|---------|
|2026年 10月27日  17時34分31秒|LO 演出チェック|承認して監督に回す|peter|
```

- Markdown の表の形式で、1回の操作が1行です。新しい行は、ファイルの最後に追記されます。
- 記録される内容は、[バックアップと復旧](../administration/backup.ja.md) の「ログから経緯をたどる」を参照してください。

## そのほかのファイル

| ファイル | 説明 |
|---|---|
| `conf.ini` | Stuff フォルダの場所（[Stuff フォルダ](../administration/stuff-folder.ja.md) を参照） |
| `defaultusersettings.ini` | 環境設定の初期値（[Stuff フォルダ](../administration/stuff-folder.ja.md) を参照） |
| `usersettings.ini` | 各 PC の環境設定（[環境設定](../using/preferences.ja.md) を参照） |
| `google_spreadsheet_cues` フォルダのファイル | Google スプレッドシートへの書き込みの記録（[Google スプレッドシート連携](../integrations/google-spreadsheet.ja.md) を参照） |
| `google_chat/google_chat.log` | Google Chat への通知の記録（[Google Chat 通知](../integrations/google-chat.ja.md) を参照） |
| リネームスクリプト（`.js`） | [リネームスクリプト](../integrations/rename-scripts.ja.md) を参照 |
| 翻訳ファイル（`.qm`） | [UI の言語と翻訳ファイル](../administration/translations.ja.md) を参照 |

---

[← 目次に戻る](../index.ja.md)
