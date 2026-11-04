#pragma once

// メインウィンドウ（MyWindow）向けのフローチャートUI一式。
// ProjectSettingsDialog側のフローチャート（flowchartscene.h/.cpp、tmpSettingsを編集する）とは異なり、
// こちらはWorkflowManager::instance()が保持する「現在ロード中のプロジェクト」をライブに参照するだけの
// 読み取り専用寄りの表示（ノードの移動・展開はできない。チェックボックス操作のみ）。
// 実際のシーン構築（MyWindowの各QButtonGroupへのチェックボックス登録等）はmywindow.cppが行い、
// ここではその部品（アイテムクラス・パン/ズームビュー・役職パレット・共通アルゴリズム）のみを提供する。

#include <QGraphicsView>
#include <QGraphicsProxyWidget>
#include <QGraphicsPathItem>
#include <QWidget>
#include <QList>
#include <QMap>
#include <QPoint>
#include <QPointF>
#include <QColor>
#include <functional>

#include "workflow.h"

class QResizeEvent;
class QMouseEvent;
class QWheelEvent;
class QPaintEvent;
class MainRolePaletteWidget;
class QAbstractButton;
class QPainter;
class QStyleOptionGraphicsItem;

//--------------------------------
// 工程ノード。中身（チェックボックス＋工程名＋カット数）はmywindow.cpp側で組み立てたQWidgetを
// setWidget()で埋め込む。角丸背景はQSSのborder-radiusに頼らず自前でpaint()するため
// （QGraphicsProxyWidget上ではスタイルシートのborder-radiusが確実に反映されないため）、
// 埋め込むcontentWidget自体は背景を透明にしておくこと。ドラッグ不可（既定のまま）
class MainWorkflowNodeItem : public QGraphicsProxyWidget {
public:
  MainWorkflowNodeItem(WorkflowType wfType, const QColor& bgColor,
                       QWidget* contentWidget);
  WorkflowType workflowType() const { return m_wfType; }

  QPointF inputPortScenePos() const;   // 上辺中央
  QPointF outputPortScenePos() const;  // 下辺中央

  // カット検索結果、このノードが検索されたカットの所属工程であることを示す強調表示のON/OFF。
  // 強調表示は輪郭線の外側に描くため、boundingRect()はその分だけ常に広めに確保している
  void setHighlighted(bool highlighted) {
    m_highlighted = highlighted;
    update();
  }
  bool isHighlighted() const { return m_highlighted; }

  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

  // ウィジェット自体の矩形（ローカル座標(0,0)起点）。ポート位置の計算・通常の背景描画・
  // グループ枠の大きさの計算は、強調表示用に広げたboundingRect()ではなくこちらを基準にする
  QRectF widgetRect() const;

private:
  WorkflowType m_wfType;
  QColor m_bgColor;
  bool m_highlighted = false;
};

//--------------------------------
// 工程グループの背景枠。中身（3stateチェックボックス＋カット数＋reloadボタンの行）は
// mywindow.cpp側で組み立てたQWidgetを埋め込む。背景の角丸は自前でpaint()する
// （理由はMainWorkflowNodeItem参照）
class MainGroupNodeItem : public QGraphicsProxyWidget {
public:
  MainGroupNodeItem(FlowGroupId fgId, QWidget* contentWidget);
  FlowGroupId groupId() const { return m_fgId; }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

private:
  FlowGroupId m_fgId;
};

//--------------------------------
// リンク（アクションのnextFlows→後工程）。編集用と異なり、常に(from,to)ペアで1本に集約し、
// 警告表示や選択・削除は行わない単純な表示専用の曲線
class MainLinkItem : public QGraphicsPathItem {
public:
  MainLinkItem();
  void setEndpoints(const QPointF& start, const QPointF& end);
};

//--------------------------------
class MainFlowChartView : public QGraphicsView {
  Q_OBJECT
public:
  MainFlowChartView(QWidget* parent = nullptr);

  // 役職パレット（生成・所有はMyWindow側。MyWindow::m_buttonGroupへの参照が要るため）を
  // このビューの子として右下に固定表示する
  void attachRolePalette(MainRolePaletteWidget* rolePalette);

  // sceneRectの中心がビューの中心になるようパンし、sceneRect全体が収まるよう表示倍率を変更する
  // （ただし100%（等倍）より拡大はしない）。カット検索結果のハイライト表示と合わせて使う
  void focusOnRect(const QRectF& sceneRect);

protected:
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  bool m_panning = false;
  QPoint m_panLastPos;
  MainRolePaletteWidget* m_rolePalette = nullptr;
};

//--------------------------------
// 役職パレット：クリックした役職の色を持つ工程を一括選択／選択解除する3state切替ボタンとして働く。
// 対象の判定はsetRoleButtonCache()で渡されたキャッシュを通じて行う。
// 選択状態を変更した後はonSelectionChangedを呼ぶ（MyWindow::refreshList()を呼んでもらう想定）
class MainRolePaletteWidget : public QWidget {
  Q_OBJECT
public:
  MainRolePaletteWidget(QWidget* parent = nullptr);

  void setOnSelectionChanged(std::function<void()> callback) {
    m_onSelectionChanged = callback;
  }
  // 役職の色(QRgb)→対応する工程のチェックボックス一覧、のキャッシュを設定する。
  // rebuildFlowchartPanel()で一度だけ構築して渡してもらう。これにより、クリック／再描画のたびに
  // WorkflowManager::workflow(type)（Workflow構造体のディープコピーを伴う）を呼ばずに済む
  void setRoleButtonCache(const QMap<QRgb, QList<QAbstractButton*>>& cache) {
    m_roleButtonCache = cache;
    update();
  }
  // tmpSettingsではなくWorkflowManager::instance()->roles()の内容から表示を作り直す
  void refresh();
  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

private:
  QRectF roleIconRect(int index) const;
  int roleIndexAt(const QPoint& pos) const;
  // index番目の役職の色を持つ工程のチェックボックス一覧を、m_roleButtonCacheから返す
  QList<QAbstractButton*> buttonsForRole(int index) const;

  std::function<void()> m_onSelectionChanged;
  QMap<QRgb, QList<QAbstractButton*>> m_roleButtonCache;
};

//--------------------------------
// 同一工程グループ内で、nextFlowsで繋がっている工程の連結成分ごとにまとめたWorkflowTypeのリストを返す
// （flowchartscene.cppのFlowChartScene::recomputeGroupNodeEntries()と同じアルゴリズムの、
//  WorkflowManager::instance()のデータに対する版。tmpSettingsを編集しないため関数として独立させている）
QList<QList<WorkflowType>> groupWorkflowsByConnectedComponent(FlowGroupId fgId);

// レイアウトが保存されていないプロジェクト向けに、nextFlowsの接続からノード配置を自動生成する
// （flowchartscene.cppのbuildDefaultFlowChartLayout()のWorkflowManagerデータ版）
FlowChartLayout buildDefaultMainFlowChartLayout();
