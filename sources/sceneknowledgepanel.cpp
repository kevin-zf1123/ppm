#include "sceneknowledgepanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QGroupBox>
#include <QFrame>
#include <QFont>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPixmap>
#include <QPalette>
#include <QColor>

namespace {
const int ThumbnailSize = 160;

QLabel* buildFormLabel(const QFont& font) {
  QLabel* label = new QLabel();
  label->setWordWrap(true);
  label->setFont(font);
  return label;
}

// 所属工程（進捗ステップ）／傍流工程グループの表示に使う、複数行を縦に並べるコンテナ。
// 中身は毎回作り直す（QVBoxLayoutをコンテナのすぐ下に返す）
QWidget* buildListContainer(QVBoxLayout*& outLayout) {
  QWidget* container = new QWidget();
  outLayout          = new QVBoxLayout(container);
  outLayout->setContentsMargins(0, 0, 0, 0);
  outLayout->setSpacing(3);
  return container;
}

// outLayoutの中身を全て破棄する
void clearListContainer(QVBoxLayout* layout) {
  while (QLayoutItem* item = layout->takeAt(0)) {
    delete item->widget();
    delete item;
  }
}
}  // namespace

SceneKnowledgePanel::SceneKnowledgePanel(QWidget* parent) : QWidget(parent) {
  m_titleLabel    = new QLabel(this);
  QFont titleFont = m_titleLabel->font();
  titleFont.setPointSize(titleFont.pointSize() + 6);
  titleFont.setBold(true);
  m_titleLabel->setFont(titleFont);

  // 各情報項目のフォントサイズは、カット名（タイトル）と同じ大きさに揃える（太字にはしない）
  QFont infoFont = m_titleLabel->font();
  infoFont.setBold(false);

  QPushButton* closeButton = new QPushButton(tr("✕"), this);  // ✕
  closeButton->setFixedSize(56, 56);  // 従来(28x28)のおよそ2倍
  QFont closeFont = closeButton->font();
  closeFont.setPointSize(closeFont.pointSize() + 6);
  closeButton->setFont(closeFont);
  closeButton->setToolTip(tr("Close"));
  connect(closeButton, &QPushButton::clicked, this,
          &SceneKnowledgePanel::closeRequested);

  m_thumbnailLabel = new QLabel(this);
  m_thumbnailLabel->setFixedSize(ThumbnailSize, ThumbnailSize);
  m_thumbnailLabel->setAlignment(Qt::AlignCenter);
  m_thumbnailLabel->setFrameShape(QFrame::Box);

  m_mainGroupLabel       = buildFormLabel(infoFont);
  m_workflowContainer    = buildListContainer(m_workflowLayout);
  m_branchGroupContainer = buildListContainer(m_branchGroupLayout);
  m_takeInfoLabel        = buildFormLabel(infoFont);

  QFormLayout* infoFormLay = new QFormLayout();
  infoFormLay->setContentsMargins(0, 0, 0, 0);
  QLabel* mainGroupCaption   = new QLabel(tr("Main Flow Group"));
  QLabel* branchGroupCaption = new QLabel(tr("Branch Flow Group"));
  QLabel* workflowCaption    = new QLabel(tr("Workflow"));
  QLabel* takeInfoCaption    = new QLabel(tr("Current Take"));
  mainGroupCaption->setFont(infoFont);
  branchGroupCaption->setFont(infoFont);
  workflowCaption->setFont(infoFont);
  takeInfoCaption->setFont(infoFont);
  infoFormLay->addRow(mainGroupCaption, m_mainGroupLabel);
  infoFormLay->addRow(branchGroupCaption, m_branchGroupContainer);
  infoFormLay->addRow(workflowCaption, m_workflowContainer);
  infoFormLay->addRow(takeInfoCaption, m_takeInfoLabel);

  QHBoxLayout* topLay = new QHBoxLayout();
  topLay->setContentsMargins(0, 0, 0, 0);
  topLay->setSpacing(15);
  topLay->addWidget(m_thumbnailLabel, 0);
  topLay->addLayout(infoFormLay, 1);

  m_infoTagsLabel = new QLabel(this);
  m_infoTagsLabel->setTextFormat(Qt::RichText);
  m_infoTagsLabel->setWordWrap(true);
  m_infoTagsLabel->setFont(infoFont);
  QGroupBox* infoTagsBox   = new QGroupBox(tr("Scene Info Tags"), this);
  QVBoxLayout* infoTagsLay = new QVBoxLayout(infoTagsBox);
  infoTagsLay->addWidget(m_infoTagsLabel);

  // カット履歴（ログ）は情報量が多いため、従来通りの文字サイズのままにする
  m_historyEdit = new QTextEdit(this);
  m_historyEdit->setReadOnly(true);
  QGroupBox* historyBox   = new QGroupBox(tr("History"), this);
  QVBoxLayout* historyLay = new QVBoxLayout(historyBox);
  historyLay->addWidget(m_historyEdit);

  QWidget* contentWidget  = new QWidget();
  QVBoxLayout* contentLay = new QVBoxLayout(contentWidget);
  contentLay->setContentsMargins(10, 10, 10, 10);
  contentLay->setSpacing(15);
  contentLay->addLayout(topLay, 0);
  contentLay->addWidget(infoTagsBox, 0);
  contentLay->addWidget(historyBox, 1);

  QScrollArea* scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setWidget(contentWidget);

  QHBoxLayout* headerLay = new QHBoxLayout();
  headerLay->setContentsMargins(10, 10, 10, 0);
  headerLay->addWidget(m_titleLabel, 1);
  headerLay->addWidget(closeButton, 0);

  QVBoxLayout* mainLay = new QVBoxLayout(this);
  mainLay->setContentsMargins(0, 0, 0, 0);
  mainLay->setSpacing(5);
  mainLay->addLayout(headerLay, 0);
  mainLay->addWidget(scrollArea, 1);
}

bool SceneKnowledgePanel::loadScene(const QString& cutName) {
  m_foundWorkflowTypes.clear();

  QString sequence = WorkflowManager::instance()->sceneNum2Sequence(cutName);

  // 主流工程グループ（1カットにつき1つ）を探索する
  FlowGroupId mainGroupId = Group_None;
  for (auto fgId : WorkflowManager::instance()->flowGroupIds()) {
    if (WorkflowManager::instance()->cutListInFlow(fgId).contains(cutName)) {
      mainGroupId = fgId;
      break;
    }
  }

  // 傍流工程グループ（1カットが0個または複数持てる）を、全工程グループについて網羅的に探索する
  QList<FlowGroupId> branchGroupIds;
  for (auto fgId : WorkflowManager::instance()->flowGroupIds()) {
    if (WorkflowManager::instance()->cutListInBranchFlow(fgId).contains(
            cutName))
      branchGroupIds.append(fgId);
  }

  // 所属工程（進捗ステップ）：主流・傍流の複数の工程グループに同時に分岐することがあるため、
  // 全工程グループ・全Workflowについて網羅的に探索する
  QList<WorkflowType> foundWfTypes;
  for (auto fgId : WorkflowManager::instance()->flowGroupIds()) {
    QMap<WorkflowType, QStringList> progressList;
    for (int i = 0; i < WorkflowManager::instance()->workflowCount(); i++) {
      Workflow wf = WorkflowManager::instance()->getWorkflow(i);
      if (wf.groupId == fgId) progressList.insert(wf.workflowType, {});
    }
    if (progressList.isEmpty()) continue;
    WorkflowManager::instance()->getProgressList(fgId, progressList);
    for (auto wfType : progressList.keys()) {
      if (progressList.value(wfType).contains(cutName))
        foundWfTypes.append(wfType);
    }
  }

  QString logPath = WorkflowManager::instance()->logFilePath(sequence, cutName);
  bool hasHistory = QFileInfo::exists(logPath);

  // 工程グループの所属（主流・傍流いずれも）が見つからず、カット履歴も無い場合は「見つからなかった」扱いにする
  if (mainGroupId == Group_None && branchGroupIds.isEmpty() && !hasHistory)
    return false;

  m_foundWorkflowTypes = foundWfTypes;

  m_titleLabel->setText(tr("Scene: %1").arg(cutName));

  // サムネイル：Scene Thumbnail Pathが設定されているプロジェクトでのみ表示する
  QString thumbnailPathTemplate =
      WorkflowManager::instance()->sceneThumbnailPath();
  if (thumbnailPathTemplate.isEmpty()) {
    m_thumbnailLabel->setVisible(false);
  } else {
    m_thumbnailLabel->setVisible(true);
    QString thumbnailPath =
        WorkflowManager::instance()->sceneThumbnailPathFor(sequence, cutName);
    QImage image =
        (!thumbnailPath.isEmpty() && QFileInfo::exists(thumbnailPath))
            ? QImage(thumbnailPath)
            : QImage();
    if (!image.isNull()) {
      m_thumbnailLabel->setText(QString());
      m_thumbnailLabel->setPixmap(QPixmap::fromImage(image).scaled(
          ThumbnailSize - 4, ThumbnailSize - 4, Qt::KeepAspectRatio,
          Qt::SmoothTransformation));
    } else {
      m_thumbnailLabel->setPixmap(QPixmap());
      m_thumbnailLabel->setText(tr("No thumbnail"));
    }
  }

  m_mainGroupLabel->setText(mainGroupId != Group_None
                                ? WorkflowManager::instance()
                                      ->flowGroupInfos()
                                      .value(mainGroupId)
                                      .labelName
                                : tr("None"));

  // 傍流工程グループ：複数あり得るため、1行ずつ並べる
  clearListContainer(m_branchGroupLayout);
  if (branchGroupIds.isEmpty()) {
    QLabel* label = buildFormLabel(m_mainGroupLabel->font());
    label->setText(tr("None"));
    m_branchGroupLayout->addWidget(label);
  } else {
    for (auto fgId : branchGroupIds) {
      QLabel* label = buildFormLabel(m_mainGroupLabel->font());
      label->setText(
          WorkflowManager::instance()->flowGroupInfos().value(fgId).labelName);
      m_branchGroupLayout->addWidget(label);
    }
  }

  // 所属工程（進捗ステップ）：複数あり得るため、それぞれその工程の表示色を背景に敷いて1行ずつ並べる
  clearListContainer(m_workflowLayout);
  if (foundWfTypes.isEmpty()) {
    QLabel* label = buildFormLabel(m_mainGroupLabel->font());
    label->setText(tr("None"));
    m_workflowLayout->addWidget(label);
  } else {
    for (auto wfType : foundWfTypes) {
      Workflow wf   = WorkflowManager::instance()->workflow(wfType);
      QLabel* label = buildFormLabel(m_mainGroupLabel->font());
      label->setText(wf.name);
      label->setAutoFillBackground(true);
      QPalette pal = label->palette();
      pal.setColor(QPalette::Window, wf.color);
      pal.setColor(QPalette::WindowText, (wf.color.lightness() > 128)
                                             ? QColor(25, 25, 25)
                                             : QColor(240, 240, 240));
      label->setPalette(pal);
      label->setContentsMargins(6, 2, 6, 2);
      label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
      label->setWordWrap(false);
      m_workflowLayout->addWidget(label);
    }
  }

  // 現在のテイク情報
  int currentTake = WorkflowManager::instance()->currentTake(cutName);
  QVariantMap retakeInfo =
      WorkflowManager::instance()->obtainRetakeInfo(cutName);
  if (currentTake < 0) {
    m_takeInfoLabel->setText(tr("None"));
  } else if (retakeInfo.isEmpty()) {
    m_takeInfoLabel->setText(tr("Take %1").arg(currentTake));
  } else {
    m_takeInfoLabel->setText(
        tr("Take %1 (Passed To: %2, Date: %3, Comment: %4)")
            .arg(currentTake)
            .arg(retakeInfo.value("PassedTo").toString())
            .arg(retakeInfo.value("DateTime").toString())
            .arg(retakeInfo.value("Comment").toString()));
  }

  // カット情報タグ
  QString infoTagsTxt =
      WorkflowManager::instance()->getSceneInfoTagsRichText(sequence, cutName);
  m_infoTagsLabel->setText(infoTagsTxt.isEmpty() ? tr("None") : infoTagsTxt);

  // カット履歴（ログのMarkdown）
  if (hasHistory) {
    QFile file(logPath);
    if (file.open(QFile::ReadOnly | QFile::Text))
      m_historyEdit->setMarkdown(file.readAll());
    else
      m_historyEdit->setPlainText(tr("None"));
  } else {
    m_historyEdit->setPlainText(tr("None"));
  }

  return true;
}
