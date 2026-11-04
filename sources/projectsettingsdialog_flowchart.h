#pragma once

// ProjectSettingsDialogの「Workflows」カテゴリの隣に追加する「Workflows -
// Flowchart」カテゴリ用ページ。
// WorkflowPageのカラム①（工程一覧）だけをフローチャートUIに差し替えたもので、
// カラム②③（Workflow詳細＋Protocol/Action/Step編集）はWorkflowPageをそのまま再利用する
// （一覧カラムだけ非表示にして埋め込み、選択はフローチャート側のノードダブルクリックから与える）。
// 工程グループノードのダブルクリックでは、同様にWfGroupsPageを再利用した詳細パネルを開く。

#include <QWidget>

#include "workflow.h"

class FlowChartScene;
class FlowChartView;
class WorkflowPage;
class WfGroupsPage;
class QStackedWidget;

class FlowchartPage : public QWidget {
  Q_OBJECT

  FlowChartScene* m_scene;
  FlowChartView* m_view;
  WorkflowPage* m_workflowPage;
  WfGroupsPage* m_wfGroupsPage;
  QStackedWidget* m_detailStack;
  QWidget* m_placeholderPage;

public:
  FlowchartPage(QWidget* parent = nullptr);

  // tmpSettingsの内容からフローチャート・詳細パネルを作り直す
  void refresh();
  // シーン自身の編集操作以外（他カテゴリでの工程編集等）でtmpSettingsが変わった際に、
  // フローチャート表示を作り直す。ProjectSettingsDialogのg_onDirtyChangedフックから呼ばれる
  void rebuildIfNotSelfEditing();

protected slots:
  void onNodeDoubleClicked(WorkflowType wfType);
  void onGroupNodeDoubleClicked(FlowGroupId fgId);
};
