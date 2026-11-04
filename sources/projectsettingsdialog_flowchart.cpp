#include "projectsettingsdialog_flowchart.h"
#include "projectsettingsdialog.h"  // WorkflowPage / WfGroupsPage
#include "projectsettings_common.h"
#include "flowchartscene.h"
#include "workflow.h"

#include <QVBoxLayout>
#include <QSplitter>
#include <QStackedWidget>
#include <QLabel>

FlowchartPage::FlowchartPage(QWidget* parent) : QWidget(parent) {
  m_scene = new FlowChartScene(this);
  m_view  = new FlowChartView(this);
  m_view->setScene(m_scene);

  // 詳細パネル：既存のページを再利用する。
  // WorkflowPageの工程一覧カラムは使わず、選択はフローチャート側から与える
  m_workflowPage = new WorkflowPage(this);
  m_workflowPage->setListColumnVisible(false);
  m_wfGroupsPage = new WfGroupsPage(this);

  m_placeholderPage = new QWidget(this);
  {
    QVBoxLayout* lay = new QVBoxLayout(m_placeholderPage);
    QLabel* hint     = new QLabel(
        tr("Right-click the empty area or a group frame to add a workflow "
           "node,\n"
           "and drag from the bottom of a node to link workflows.\n\n"
                   "Double-click a workflow node or a group frame\n"
                   "to edit its settings here."),
        m_placeholderPage);
    hint->setAlignment(Qt::AlignCenter);
    lay->addWidget(hint, 1);
  }

  m_detailStack = new QStackedWidget(this);
  m_detailStack->addWidget(m_placeholderPage);
  m_detailStack->addWidget(m_workflowPage);
  m_detailStack->addWidget(m_wfGroupsPage);

  QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
  splitter->addWidget(m_view);
  splitter->addWidget(m_detailStack);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 1);

  QVBoxLayout* mainLay = new QVBoxLayout(this);
  mainLay->setContentsMargins(0, 0, 0, 0);
  mainLay->addWidget(splitter);

  connect(m_scene, &FlowChartScene::nodeDoubleClicked, this,
          &FlowchartPage::onNodeDoubleClicked);
  connect(m_scene, &FlowChartScene::groupNodeDoubleClicked, this,
          &FlowchartPage::onGroupNodeDoubleClicked);
}

void FlowchartPage::refresh() {
  // レイアウトが保存されていないプロジェクトは、nextFlowsの接続から自動レイアウトを生成する。
  // （この時点ではdirty扱いにしない：Saveすれば以降は保存されたレイアウトが使われる）
  if (tmpSettings.flowChartLayout.isEmpty() && !tmpSettings.workflows.isEmpty())
    tmpSettings.flowChartLayout = buildDefaultFlowChartLayout();

  m_workflowPage->refresh();
  m_wfGroupsPage->refresh();
  m_detailStack->setCurrentWidget(m_placeholderPage);

  m_scene->rebuildFromModel();
  m_view->rolePalette()->refresh();
}

void FlowchartPage::rebuildIfNotSelfEditing() {
  if (!m_scene->isSelfEditing()) m_scene->scheduleRebuild();
}

void FlowchartPage::onNodeDoubleClicked(WorkflowType wfType) {
  m_detailStack->setCurrentWidget(m_workflowPage);
  m_workflowPage
      ->refresh();  // フローチャート上で工程が追加されている場合に備えて一覧を作り直す
  m_workflowPage->selectWorkflowByType(wfType);
}

void FlowchartPage::onGroupNodeDoubleClicked(FlowGroupId fgId) {
  m_detailStack->setCurrentWidget(m_wfGroupsPage);
  m_wfGroupsPage
      ->refresh();  // グループが新規作成されている場合に備えて一覧を作り直す
  m_wfGroupsPage->selectGroup(fgId);
}
