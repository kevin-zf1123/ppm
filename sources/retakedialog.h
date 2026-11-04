#pragma once

#include <QDialog>
#include <QMap>
#include <QStringList>

#include "workflow.h"

class QLineEdit;
class QLabel;
class QButtonGroup;
class QTextEdit;
class QPushButton;
class QComboBox;
class QHBoxLayout;

class RetakeDialog : public QDialog {
  Q_OBJECT

  QLineEdit* m_cutNameEdit;

  enum TakeNumPolicy { TakeNumPolicy_Auto, TakeNumPolicy_Manual };
  // テイク番号を自動で付けるか、手動で指定するか
  QComboBox* m_takeNumPolicyCombo;
  QLineEdit* m_takeNumEdit;

  QLabel* m_infoLabel;
  QButtonGroup* m_passToButtonGroup;
  QHBoxLayout* m_passToLay;
  QTextEdit* m_commentEdit;

  QPushButton* m_retakeButton;

  QMap<int, QStringList>
      m_cutListByTakeNum;  // リテイク後のテイク番号をKeyにしてカット一覧をまとめる
public:
  RetakeDialog(QWidget* parent = nullptr);

  void setInfoLabel(const QString, const QColor color = QColor(Qt::black));
  // 送り先（リテイク工程グループ）のボタンを、現在のプロジェクトに合わせて作り直す
  void refresh();
signals:
  // カットの所属が変わり、これらの工程グループの詳細進捗を作り直す必要がある
  void flowGroupsChanged(const QList<FlowGroupId>& fgIds);
protected slots:
  void onCutNameEdited();
  void onRetake();
  void onTakeNumPolicyChanged();
};