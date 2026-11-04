#include "retakedialog.h"

#include "workflow.h"
#include "actionconfirmationdialog.h"

#include <iostream>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QRegularExpression>
#include <QButtonGroup>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QDesktopServices>
#include <QIntValidator>

namespace {
// リテイク中のカットの目印にするカット情報のID名。プロジェクトにこのカット情報が
// 定義されている場合、リテイクを出したときに、リテイク先の工程グループ名を値として付ける
// （外すのは、リテイク工程の最後のアクションの「カット情報を登録」ステップで行う）
const QString RetakeWipTagId = "RETAKE_WIP";

bool projectHasSceneInfoTag(const QString& idName) {
  for (auto tag : WorkflowManager::instance()->sceneInfoTagList())
    if (tag->idName() == idName) return true;
  return false;
}
}  // namespace

RetakeDialog::RetakeDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(tr("Issue New Retakes"));
  m_cutNameEdit = new QLineEdit(this);

  m_takeNumPolicyCombo = new QComboBox(this);
  m_takeNumEdit        = new QLineEdit(this);

  m_infoLabel         = new QLabel(this);
  m_passToButtonGroup = new QButtonGroup(this);
  m_commentEdit       = new QTextEdit(this);

  m_retakeButton           = new QPushButton(tr("Retake"), this);
  QPushButton* closeButton = new QPushButton(tr("Close"), this);

  m_takeNumPolicyCombo->addItem(tr("Auto"), TakeNumPolicy_Auto);
  m_takeNumPolicyCombo->addItem(tr("Manual"), TakeNumPolicy_Manual);
  m_takeNumPolicyCombo->setCurrentIndex(
      m_takeNumPolicyCombo->findData(TakeNumPolicy_Auto));
  m_takeNumEdit->setValidator(new QIntValidator(2, 100));
  m_takeNumEdit->setText("2");
  m_takeNumEdit->setDisabled(true);
  m_takeNumEdit->setFixedWidth(50);

  m_cutNameEdit->setStyleSheet("QLineEdit { font : bold 20px; }");
  m_takeNumEdit->setStyleSheet("QLineEdit { font : bold 15px; }");
  m_retakeButton->setFocusPolicy(Qt::NoFocus);
  closeButton->setFocusPolicy(Qt::NoFocus);

  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setContentsMargins(15, 15, 15, 15);
  mainLay->setSpacing(10);
  {
    mainLay->addWidget(new QLabel(tr("Separate cut names by comma, colon or "
                                     "semicolon. (Ex. 0001;0002;0003)"),
                                  this),
                       0, Qt::AlignLeft | Qt::AlignVCenter);
    mainLay->addWidget(m_cutNameEdit, 0);

    mainLay->addSpacing(5);

    QHBoxLayout* takeNumLay = new QHBoxLayout();
    takeNumLay->setContentsMargins(0, 0, 0, 0);
    takeNumLay->setSpacing(5);
    {
      takeNumLay->addWidget(new QLabel(tr("Take Number:"), this), 0);
      takeNumLay->addWidget(m_takeNumPolicyCombo, 0);
      takeNumLay->addWidget(m_takeNumEdit, 0);
      takeNumLay->addWidget(m_infoLabel, 1);
    }
    mainLay->addLayout(takeNumLay, 0);

    mainLay->addSpacing(5);

    mainLay->addWidget(new QLabel(tr("Pass to:"), this), 0, Qt::AlignLeft);
    // 送り先ボタンは、プロジェクトに合わせてrefresh()で作る
    m_passToLay = new QHBoxLayout();
    m_passToLay->setContentsMargins(0, 0, 0, 0);
    m_passToLay->setSpacing(10);
    mainLay->addLayout(m_passToLay, 0);

    mainLay->addSpacing(5);

    mainLay->addWidget(new QLabel(tr("Comment:"), this), 0, Qt::AlignLeft);

    mainLay->addWidget(m_commentEdit, 1);

    QFrame* sepLine = new QFrame(this);
    sepLine->setFrameStyle(QFrame::HLine);
    mainLay->addWidget(sepLine, 0);

    QHBoxLayout* buttonsLay = new QHBoxLayout();
    buttonsLay->setContentsMargins(0, 0, 0, 0);
    buttonsLay->setSpacing(15);
    {
      buttonsLay->addStretch(1);
      buttonsLay->addWidget(m_retakeButton, 0);
      buttonsLay->addWidget(closeButton, 0);
      buttonsLay->addStretch(1);
    }
    mainLay->addLayout(buttonsLay, 0);
  }
  setLayout(mainLay);

  connect(m_cutNameEdit, SIGNAL(editingFinished()), this,
          SLOT(onCutNameEdited()));
  connect(m_retakeButton, SIGNAL(clicked()), this, SLOT(onRetake()));
  connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));
  connect(m_takeNumPolicyCombo, SIGNAL(activated(int)), this,
          SLOT(onTakeNumPolicyChanged()));
  connect(m_takeNumEdit, SIGNAL(editingFinished()), this,
          SLOT(onCutNameEdited()));

  refresh();
}

void RetakeDialog::refresh() {
  // 選ばれていた送り先を、作り直した後も選んでおく
  int prevCheckedId = m_passToButtonGroup->checkedId();

  for (auto* button : m_passToButtonGroup->buttons()) {
    m_passToButtonGroup->removeButton(button);
    delete button;
  }

  // リテイク工程グループごとに、送り先ボタンを動的に生成する
  for (auto fgId : WorkflowManager::instance()->flowGroupIds()) {
    if (!WorkflowManager::instance()->flowGroupInfos().value(fgId).isRetakeGroup)
      continue;

    QPushButton* passToButton = new QPushButton(
        tr("To %1").arg(WorkflowManager::instance()->flowGroupLabelName(fgId)),
        this);
    passToButton->setCheckable(true);
    m_passToButtonGroup->addButton(passToButton, (int)fgId);
    m_passToLay->addWidget(passToButton, 1);
  }
  QAbstractButton* toCheck = m_passToButtonGroup->button(prevCheckedId);
  if (!toCheck && !m_passToButtonGroup->buttons().isEmpty())
    toCheck = m_passToButtonGroup->buttons().first();
  if (toCheck) toCheck->setChecked(true);

  // 前のプロジェクトで入力していたカット番号は消す
  m_cutNameEdit->clear();
  onCutNameEdited();
}

void RetakeDialog::setInfoLabel(const QString txt, const QColor color) {
  m_infoLabel->setStyleSheet(QString("QLabel { color:%1; }").arg(color.name()));
  m_infoLabel->setText(txt);
}

// 撮影リテイクはPPMに乗らないままテイク番号が増えるので、手動でテイク番号を入力できるようにする
void RetakeDialog::onCutNameEdited() {
  std::cout << "onCutNameEdited" << std::endl;

  QStringList list = m_cutNameEdit->text().split(QRegularExpression(";|:|,"),
                                                 Qt::SkipEmptyParts);

  if (list.isEmpty()) {
    setInfoLabel(tr("Cut number is empty."), QColor(Qt::red));
    m_retakeButton->setEnabled(false);
    return;
  }

  TakeNumPolicy takeNumPolicy =
      (TakeNumPolicy)m_takeNumPolicyCombo->currentData().toInt();
  int manualTakeNum = -1;
  if (takeNumPolicy == TakeNumPolicy_Manual) {
    bool ok;
    manualTakeNum = m_takeNumEdit->text().toInt(&ok);
    if (!ok) {
      setInfoLabel(tr("Take number is empty."), QColor(Qt::red));
      m_retakeButton->setEnabled(false);
      return;
    }
  }

  // リテイク先（リテイク工程グループ）が無いプロジェクトでは、リテイクを出せない
  if (m_passToButtonGroup->buttons().isEmpty()) {
    setInfoLabel(tr("This project has no retake workflow group to pass the "
                    "scenes to."),
                 QColor(Qt::red));
    m_retakeButton->setEnabled(false);
    return;
  }

  // 「リテイク発行元」の工程グループに所属しているカットだけ、リテイクを出せる
  QStringList sourceCutList;
  QStringList sourceGroupNames;
  for (auto fgId : WorkflowManager::instance()->flowGroupIds()) {
    if (WorkflowManager::instance()
            ->flowGroupInfos()
            .value(fgId)
            .isRetakeSourceGroup) {
      sourceCutList += WorkflowManager::instance()->cutListInFlow(fgId);
      sourceGroupNames.append(
          WorkflowManager::instance()->flowGroupLabelName(fgId));
    }
  }
  if (sourceGroupNames.isEmpty()) {
    setInfoLabel(tr("No workflow group is set as a retake source in this "
                    "project."),
                 QColor(Qt::red));
    m_retakeButton->setEnabled(false);
    return;
  }

  // リテイクを出せないカットを、理由ごとに分ける
  QStringList notInSourceCuts, noInfoCuts, smallTakeCuts;
  m_cutListByTakeNum.clear();
  for (auto cutName : list) {
    if (!sourceCutList.contains(cutName)) {
      notInSourceCuts.append(cutName);
      continue;
    }
    // 現在のテイク＋１をした予測を出す
    int currentTake = WorkflowManager::instance()->currentTake(cutName);
    if (currentTake < 0) {
      noInfoCuts.append(cutName);
      continue;
    }

    int newTake;
    if (takeNumPolicy == TakeNumPolicy_Auto)
      newTake = currentTake + 1;
    else {  // takeNumPolicy == TakeNumPolicy_Manual
      // テイク番号は現在のテイクより大きくなくてはいけない
      if (currentTake >= manualTakeNum) {
        smallTakeCuts.append(
            tr("%1 (current: Take%2)").arg(cutName).arg(currentTake));
        continue;
      }
      newTake = manualTakeNum;
    }

    if (m_cutListByTakeNum.contains(newTake))
      m_cutListByTakeNum[newTake].append(cutName);
    else
      m_cutListByTakeNum.insert(newTake, {cutName});
  }

  // 対応できないカットが含まれる場合
  QStringList ngLines;
  if (!notInSourceCuts.isEmpty())
    ngLines.append(tr("Not in a retake source workflow group (%1): %2")
                       .arg(sourceGroupNames.join(", "),
                            notInSourceCuts.join(", ")));
  if (!noInfoCuts.isEmpty())
    ngLines.append(
        tr("Failed to read the scene info: %1").arg(noInfoCuts.join(", ")));
  if (!smallTakeCuts.isEmpty())
    ngLines.append(tr("The take number must be larger than the current take: "
                      "%1")
                       .arg(smallTakeCuts.join(", ")));
  if (!ngLines.isEmpty()) {
    setInfoLabel(ngLines.join("\n"), QColor(Qt::red));
    m_retakeButton->setEnabled(false);
    return;
  }

  // リテイク後のテイク番号をラベルに表示する
  QString labelTxt;
  bool isFirstItem = true;
  for (auto takeNum : m_cutListByTakeNum.keys()) {
    if (isFirstItem)
      isFirstItem = false;
    else
      labelTxt += "  |  ";
    labelTxt += m_cutListByTakeNum.value(takeNum).join(", ") + " -> " +
                tr("Take%1").arg(takeNum);
  }
  setInfoLabel(labelTxt, QColor(Qt::blue));
  m_retakeButton->setEnabled(true);
}

void RetakeDialog::onRetake() {
  if (m_cutListByTakeNum.isEmpty()) return;

  QStringList jumpToPaths;
  // リテイク先と、カットが元いた工程グループの詳細進捗を、後で作り直す
  QStringList allCuts;
  for (auto& cutList : m_cutListByTakeNum) allCuts.append(cutList);
  QList<FlowGroupId> affectedGroups =
      WorkflowManager::instance()->flowGroupsContainingCuts(allCuts);
  FlowGroupId passToFgId = (FlowGroupId)m_passToButtonGroup->checkedId();
  if (!affectedGroups.contains(passToFgId)) affectedGroups.append(passToFgId);
  // 失敗したカットと理由
  QStringList failedLines;
  int succeededCount = 0;
  for (auto takeNum : m_cutListByTakeNum.keys()) {
    QStringList cutList = m_cutListByTakeNum.value(takeNum);

    for (auto cut : cutList) {
      FlowGroupId fgID = (FlowGroupId)m_passToButtonGroup->checkedId();
      // カットフォルダ作成
      // （リテイク情報を記録する前に作る。作れなかった場合は、このカットには何もしない）
      QString cutFolderPath =
          WorkflowManager::instance()->createCutFolder(cut, takeNum, fgID);
      if (cutFolderPath.isEmpty()) {
        failedLines.append(
            tr("%1: Failed to create the scene folder.").arg(cut));
        continue;
      }
      // リテイク情報を記録
      if (!WorkflowManager::instance()->doRetake(
              cut, takeNum, fgID, m_commentEdit->toPlainText())) {
        failedLines.append(
            tr("%1: Failed to write the retake info to the scene info file.")
                .arg(cut));
        continue;
      }
      // リテイク中の目印（RETAKE_WIP）を付ける
      if (projectHasSceneInfoTag(RetakeWipTagId))
        WorkflowManager::instance()->saveSceneInfoTag(
            WorkflowManager::instance()->sceneNum2Sequence(cut), cut,
            RetakeWipTagId,
            WorkflowManager::instance()->flowGroupLabelName(fgID));
      // キューファイル作成
      QFile file(cutFolderPath + "/retake_start.txt");
      if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        failedLines.append(
            tr("%1: Failed to create the cue file %2. Create it by hand.")
                .arg(cut, QDir::toNativeSeparators(file.fileName())));
      file.close();

      jumpToPaths.append(cutFolderPath);
      // 工程を移動
      WorkflowManager::instance()->registerCutNames(
          {cut}, fgID, tr("Retake to Take%1").arg(takeNum));
      succeededCount++;
    }
  }

  // 失敗したカットがある場合は、先に知らせる
  if (!failedLines.isEmpty())
    QMessageBox::warning(
        this, tr("Warning"),
        tr("Some scenes could not be processed correctly:\n\n%1")
            .arg(failedLines.join("\n")));

  // 報告ダイアログ（1カットもリテイクを出せなかった場合は、上の警告だけ）
  if (succeededCount > 0) {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("Issue New Retakes"));
    msgBox.setText(tr("Retake issued."));
    QPushButton* okBtn = msgBox.addButton(tr("OK"), QMessageBox::AcceptRole);
    QPushButton* openContainingBtn = msgBox.addButton(
        tr("Open containing folder(s)"), QMessageBox::HelpRole);
    msgBox.setDefaultButton(okBtn);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.exec();

    if (msgBox.clickedButton() == openContainingBtn) {
      for (auto dirPath : jumpToPaths) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
      }
    }
  }

  // UIをClearする
  m_cutNameEdit->clear();
  m_commentEdit->clear();
  onCutNameEdited();

  emit flowGroupsChanged(affectedGroups);
}

void RetakeDialog::onTakeNumPolicyChanged() {
  m_takeNumEdit->setEnabled(m_takeNumPolicyCombo->currentData().toInt() ==
                            TakeNumPolicy_Manual);
  onCutNameEdited();
}