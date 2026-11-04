#pragma once

// ProjectSettingsDialog系（既存のページ型ダイアログとフローチャート型ダイアログ）で共有する、
// プロジェクト設定の作業用コピー（tmpSettings）とその編集状態（DirtyFlag）の管理。

#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <functional>

#include "workflow.h"

class QWidget;

// プロジェクト設定の作業用コピー。各ダイアログのページ／シーンがこれを直接読み書きし、
// Save時にWorkflowManagerへ書き戻す
struct ProjectSettings {
  QString projectTitle;
  QString projectDescription;
  QString projectRootPath;
  QString logRootPath;
  QString projectInfoFolderPath;
  QString cutNumberPrefix;
  bool useSequence =
      true;  // [SEQUENCE]マクロと、100カット毎のサブフォルダ仕分けを使うか
  QString gasUrl;
  QStringList chatDestinations;  // Google Chatの投稿先の名前の一覧（Webhook
                                 // URLはGAS側に登録する）
  QString
      sceneThumbnailPath;  // カットのサムネイル画像のパス（マクロ含む）。空ならThumbnail列は常に非表示
  bool hasEpisodes = false;
  QStringList episodeNames;
  QList<Workflow> workflows;
  // パスのマクロ
  QList<PathMacroInfo> pathMacros;
  QMap<FlowGroupId, FlowGroupInfo> flowGroupInfos;
  // カット情報
  QList<SceneInfoTag*> sceneInfoTags;
  // フローチャートUIのノード配置情報
  FlowChartLayout flowChartLayout;
  // 役職（フローチャートUIの役職パレット）
  QList<Role> roles;

  bool dirtyFlag = false;
};

extern ProjectSettings tmpSettings;

// tmpSettingsが変更されたことをダイアログのタイトルバー等へ即時反映するためのフック。
// 2種類のダイアログが共存するため、各ダイアログが開かれる（refresh()される）
// タイミングで自身のものに設定し直す
extern std::function<void()> g_onDirtyChanged;

// tmpSettingsのいずれかの値が編集されたことを示す。各編集ハンドラの末尾で呼び出す
void markDirty();

// WorkflowManagerの現在の内容をtmpSettingsへ読み込む（各ダイアログのrefresh()から呼ぶ）。
// 読み込み後、DirtyFlagはクリアされる
void refreshTmpSettingsFromProject();

// tmpSettingsの内容をWorkflowManagerへ書き戻し、projectName（trim済みでなくて良い）の
// ppmprjファイルへ保存する。プロジェクト名が空の場合や、ファイルに書き込めなかった場合は
// メッセージを出してfalseを返す。
// 成功時はDirtyFlagをクリアする（タイトルバーの更新は呼び出し側ダイアログが行う）
bool saveTmpSettingsToProject(QWidget* parent, const QString& projectName);

// 削除されるWorkflowへの参照（nextFlows／Step_ReleaseProgressステップ）を
// tmpSettings内の全Workflowから取り除く。
// 実装はprojectsettingsdialog.cpp側（ActionStepの入れ子展開処理を使うため）
struct WorkflowReferenceCleanupResult {
  int nextFlowsCount       = 0;
  int releaseProgressCount = 0;
};
WorkflowReferenceCleanupResult cleanupWorkflowReferencesInTmpSettings(
    WorkflowType deletedId);

//--------------------------------
// tmpSettings.workflowsは「工程グループのリスト順（flowGroupInfos.keys()の昇順）で
// グループ単位にクラスタリングされている」という前提を常に保つ（MyWindow::rebuildListPanel()の
// 前提と合わせるため）。WorkflowPage（一覧のドラッグ＆ドロップ／Group変更）とFlowChartUI
// （新規工程作成／グループ枠へのドラッグ）の両方から、この前提を保つために使う

// groupId（flowGroupInfosに現存する場合）の末尾に新規Workflowを挿入する際の、
// tmpSettings.workflows内の位置を返す。groupIdが未知の場合は末尾を返す
int insertPositionForWorkflowGroupEnd(FlowGroupId groupId);

// tmpSettings.workflows[currentIndex]のgroupIdをnewGroupIdに変更した上で、そのグループの
// 末尾（グループ順で見て次のグループの手前）に移動する。移動後のインデックスを返す
int moveWorkflowToGroupEnd(int currentIndex, FlowGroupId newGroupId);

//--------------------------------
// キューファイルパス／Step_Rename自動設定機能で使う共通ロジック
// （フローチャートUIの右クリックメニューと、WorkflowPageの入力補助ボタンの両方から使う）

// tmpSettings内で、指定WorkflowTypeへnextFlowsで繋がっている全アクションの位置
struct ActionLocation {
  WorkflowType workflowType = Wf_Undefined;
  int protocolIdx           = -1;
  int actionIdx             = -1;
};
QList<ActionLocation> findActionsLeadingTo(WorkflowType targetType);

// アクションのStep_Rename(全params)/Step_Copy(全params)/Step_CreateCueFile(params[0])から、
// 「このアクションの完了時に生成されるキューファイルパス」の候補を集める。
// Step_ConditionalBranchByInfo/FileExistenceについては、真偽どちらの入れ子ステップも再帰的に辿る
// （どちらか一方にのみStep_Rename等が設定されている場合も候補として拾う）
QStringList collectGeneratedCueFilePaths(const PPM_Action& action);

// pathの拡張子の直前にsuffixを挿入する（"/"より後ろの最後の"."を拡張子境界とみなす。
// 拡張子が無ければ末尾に追加する）
QString insertSuffixBeforeExtension(const QString& path, const QString& suffix);

// 「条件パスのリネーム連鎖を自動設定」の実行結果の集計
struct RenamingChainSummary {
  int stepsAdded        = 0;
  int stepsChanged      = 0;
  int conditionPathsSet = 0;
};
// clickedWfTypeの各プロトコルのConditionPathを起点に、前工程へ遡ってStep_Renameと
// ConditionPathを自動設定する。確認ダイアログはdialogParentを親にして表示する
RenamingChainSummary autoConfigureRenamingChains(QWidget* dialogParent,
                                                 WorkflowType clickedWfType);
