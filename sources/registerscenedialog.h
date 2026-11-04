#pragma once

// フローチャート上の工程ノード／工程一覧の項目を右クリックして開く、「この工程から始まるカットを
// 登録する」ためのモーダルダイアログ。既存のカット（キューファイル）をコピー、またはキューファイルを
// 新規作成し、対象の工程グループ・工程へカットを登録する一連の操作をひとつの操作にまとめる。

#include <QDialog>
#include <QWidget>
#include "workflow.h"

class QLineEdit;
class QLabel;
class QComboBox;
class QRadioButton;
class QButtonGroup;
class QPushButton;
class QDragEnterEvent;
class QDropEvent;
class QMouseEvent;

// ファイルパスをドラッグ＆ドロップ、またはクリックでファイルブラウザから選択して設定できる入力欄。
// テキストの手入力はできない（常にドロップ／ブラウザ経由）
class FileDropField : public QWidget {
  Q_OBJECT

  QLabel* m_pathLabel;
  QString m_path;

public:
  FileDropField(QWidget* parent = nullptr);
  QString path() const { return m_path; }
  void setPath(const QString& path);

signals:
  void pathChanged();

protected:
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
};

class RegisterSceneDialog : public QDialog {
  Q_OBJECT

  WorkflowType m_workflowType;

  QLineEdit* m_sceneNameEdit;
  QLabel* m_sceneNameWarningLabel;  // 使えないカット名のときに、理由を表示する
  QButtonGroup* m_methodButtonGroup;
  QRadioButton* m_copyMethodRadio;
  QRadioButton* m_emptyMethodRadio;
  FileDropField* m_sourceFileField;
  QComboBox* m_protocolCombo;
  QPushButton* m_registerButton;

  // 登録が実際に行われたかどうか、行われた場合にどの工程グループが関係したか
  // （呼び出し元がMyTable::simpleSearch()で該当グループを更新し直すために使う）
  bool m_didRegister              = false;
  FlowGroupId m_registeredGroupId = Group_None;
  FlowGroupId m_previousGroupId =
      Group_None;  // 別グループに既に登録されていた場合。無ければGroup_None

  // 現在選択中の方式（コピー／空作成）に応じたファイル拡張子から、条件パスの拡張子が一致する
  // プロトコルへコンボボックスの選択を自動的に切り替える。複数一致する場合は警告を出す
  void updateProtocolComboFromExtension();
  // 現在のシーン名・選択中プロトコルから、生成されるキューファイルのフルパスを求める。
  // シーン名が不正、またはプロトコル未選択の場合は空文字列を返す
  QString computeDestinationPath() const;
  // 現在の入力内容が登録可能な状態かどうかに応じて、Registerボタンの有効/無効を切り替える
  void updateRegisterButtonEnabled();
  // cutNameが既にいずれかの工程の詳細進捗リストに登録されているかを、全工程グループについて
  // 網羅的に探索する。見つかった場合はtrueを返し、outWfTypeにその工程を格納する
  bool findExistingRegistration(const QString& cutName,
                                WorkflowType& outWfType) const;

public:
  RegisterSceneDialog(WorkflowType workflowType, QWidget* parent = nullptr);

  bool didRegister() const { return m_didRegister; }
  FlowGroupId registeredGroupId() const { return m_registeredGroupId; }
  FlowGroupId previousGroupId() const { return m_previousGroupId; }

protected slots:
  void onMethodChanged();
  void onSourceFileChanged();
  void onInputEdited();
  void onRegisterClicked();
};
