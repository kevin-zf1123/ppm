#pragma once

// 指定したカットが現在ワークフローのどの工程にあるか、そのカットの基本情報を一覧表示するパネル。
// MyWindowの右側（通常はMyTableが表示されているエリア）に、検索結果として置き換え表示される。

#include <QWidget>
#include <QList>
#include "workflow.h"

class QLabel;
class QTextEdit;
class QPushButton;
class QVBoxLayout;

class SceneKnowledgePanel : public QWidget {
  Q_OBJECT

  QLabel* m_titleLabel;
  QLabel* m_thumbnailLabel;
  QLabel* m_mainGroupLabel;
  QWidget*
      m_branchGroupContainer;  // 傍流工程グループは0個以上あるため、行を並べるコンテナにする
  QVBoxLayout* m_branchGroupLayout;
  QWidget*
      m_workflowContainer;  // 所属工程（進捗ステップ）も複数に分岐し得るため、同様にコンテナにする
  QVBoxLayout* m_workflowLayout;
  QLabel* m_takeInfoLabel;
  QLabel* m_infoTagsLabel;
  QTextEdit* m_historyEdit;

  // 検索してカットが見つかった場合、その所属工程（進捗ステップ）の一覧。見つからない場合は空。
  // フローチャートUIでのハイライト対象を、呼び出し側（MyWindow）へ伝えるために保持する
  QList<WorkflowType> m_foundWorkflowTypes;

public:
  SceneKnowledgePanel(QWidget* parent = nullptr);

  // cutNameを現在のプロジェクトから検索し、見つかった場合はパネルの内容を作り直してtrueを返す。
  // 工程グループへの所属（主流・傍流のいずれも）が見つからず、カット履歴も存在しない場合は
  // 内容を作り直さずfalseを返す（この場合、呼び出し側はパネルを表示せず警告を出す）
  bool loadScene(const QString& cutName);

  // loadScene()が見つけた、このカットが現在属する工程（進捗ステップ）の一覧。無ければ空
  QList<WorkflowType> foundWorkflowTypes() const {
    return m_foundWorkflowTypes;
  }

signals:
  // ✕ボタンが押された。MyWindow側でMyTableの表示に戻す
  void closeRequested();
};
