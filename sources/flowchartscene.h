#pragma once

// フローチャートUI：制作工程（Workflow）をノードとして配置し、
// アクションのnextFlowsをリンクとして表示・編集するQGraphicsScene一式。
// データはprojectsettings_common.hのtmpSettingsを直接読み書きする。

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QColor>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QSizeF>
#include <QWidget>
#include <QPoint>

#include "workflow.h"

class FlowChartScene;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneContextMenuEvent;
class QPaintEvent;
class QContextMenuEvent;
class QResizeEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

// プロトコル内のアクションを指す識別子（tmpSettings内のインデックス）
struct ActionRef {
  int protocolIdx = -1;
  int actionIdx   = -1;
  bool isValid() const { return protocolIdx >= 0 && actionIdx >= 0; }
  bool operator==(const ActionRef& o) const {
    return protocolIdx == o.protocolIdx && actionIdx == o.actionIdx;
  }
};

//--------------------------------
// 工程ノード。折りたたみ時は工程名のみの横長角丸矩形、
// 展開時はプロトコル箱（条件パス）とアクションチップを内部に表示する
class WorkflowNodeItem : public QGraphicsItem {
public:
  WorkflowNodeItem(FlowChartScene* chartScene, WorkflowType wfType);

  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

  WorkflowType workflowType() const { return m_wfType; }
  bool isExpanded() const;
  QSizeF nodeSize() const { return m_size; }

  // リンクの接続位置（シーン座標）
  QPointF inputPortScenePos() const;   // 上辺中央
  QPointF outputPortScenePos() const;  // 折りたたみ時：下辺中央
  QPointF actionPortScenePos(
      const ActionRef& ref) const;  // 展開時：アクションチップの下辺中央
  QColor actionColor(const ActionRef& ref) const;

  // tmpSettingsの現在の内容からジオメトリ（サイズ・チップ配置）を計算し直す
  void rebuildGeometry();

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
  QVariant itemChange(GraphicsItemChange change,
                      const QVariant& value) override;

private:
  const Workflow* findWorkflow() const;
  QRectF toggleRect() const;
  // itemPos上のアクションチップを返す（無ければ無効なActionRef）
  ActionRef actionChipAt(const QPointF& itemPos) const;
  // itemPos上に応じてツールチップを動的に切り替える（未設定のCondition
  // Pathがある箇所の説明用）
  void updateHoverTooltip(const QPointF& itemPos);

  FlowChartScene* m_chartScene;
  WorkflowType m_wfType;
  QSizeF m_size;

  struct ProtocolBoxGeom {
    QRectF rect;
    QString label;     // 省略表示用
    QString fullText;  // ツールチップ用の条件パス全文（未設定なら空文字列）
  };
  struct ActionChipGeom {
    QRectF rect;
    QString label;
    QColor color;
    ActionRef ref;
  };
  QList<ProtocolBoxGeom> m_protocolBoxes;
  QList<ActionChipGeom> m_actionChips;
  // 折りたたみ時のハイライト判定用：いずれかのプロトコルのCondition
  // Pathが未設定か
  bool m_hasEmptyConditionPath = false;

  bool m_moved = false;
};

//--------------------------------
// 工程グループの背景枠ノード。工程ノードの下に敷かれる。
// 位置・サイズは「枠に含まれる工程ノード全体のバウンディングボックス＋マージン」から自動導出され、
// 同一グループ内でリンクの繋がっている工程ノードのまとまり（連結成分）ごとに1枠が作られる（飛び地対応）。
// 所属する工程ノードが1つも無いグループのみ、手動配置のプレースホルダー枠として扱う。
// ラベル帯のドラッグで、枠内かつ同グループ所属の工程ノードをまとめて移動できる
class GroupNodeItem : public QGraphicsItem {
public:
  GroupNodeItem(FlowChartScene* chartScene, int layoutIndex);

  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

  FlowGroupId groupId() const;
  int layoutIndex() const { return m_layoutIndex; }

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  QVariant itemChange(GraphicsItemChange change,
                      const QVariant& value) override;

private:
  FlowChartGroupNode* layoutNode() const;

  FlowChartScene* m_chartScene;
  int m_layoutIndex;  // tmpSettings.flowChartLayout.groupNodes内のインデックス
  QSizeF m_size;

  bool m_draggingByMouse = false;
  QPointF m_lastPos;
  QList<WorkflowNodeItem*>
      m_draggedMembers;  // ドラッグ開始時に枠内にあった、同グループ所属の工程ノード
  bool m_moved = false;
};

//--------------------------------
// リンク（アクションのnextFlows→後工程）。
// 接続元が展開中はアクション1つにつき1本、折りたたみ中は(from,to)ペアで1本に集約する
class LinkItem : public QGraphicsPathItem {
public:
  LinkItem(FlowChartScene* chartScene, WorkflowType from, WorkflowType to);

  WorkflowType fromType() const { return m_from; }
  WorkflowType toType() const { return m_to; }

  QList<ActionRef> actionRefs;  // このリンクに対応するアクション
  QColor color;
  // actionRefsのいずれかのアクションが、後工程のCondition
  // Pathに一致するファイルを
  // 生成する設定（Step_Rename/Step_Copy/Step_CreateCueFile）を持っていない場合true
  bool warning = false;
  // 全てのactionRefsが、条件分岐の中のStep_AddProgressで繋がっている場合true（破線で描く）
  bool conditional = false;

  // 両端ノードの現在位置からパスを引き直す（1対1直結時は非表示にする）
  void updatePath();

  QPainterPath shape()
      const override;  // クリックしやすいよう太めの当たり判定にする
  // 選択状態をペン色・太さで表現する（既定の選択点線枠は使わない）
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

private:
  FlowChartScene* m_chartScene;
  WorkflowType m_from, m_to;
};

//--------------------------------
// リンクの警告マーク（warning.svg）。リンク線が直結表示で非表示になっている場合でも
// 独立したアイテムとして常に表示されるよう、LinkItemとは別アイテムにしている
class WarningMarkItem : public QGraphicsItem {
public:
  WarningMarkItem(LinkItem* link, const QString& tooltipText);

  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

  // リンクの現在の描画状態（直結中か、パスの中点か）から表示位置を求め直す
  void updatePosition(FlowChartScene* chartScene);

private:
  LinkItem* m_link;
};

//--------------------------------
class FlowChartScene : public QGraphicsScene {
  Q_OBJECT
public:
  FlowChartScene(QObject* parent = nullptr);

  // tmpSettings（workflows＋flowChartLayout）から全アイテムを作り直す
  void rebuildFromModel();
  // 次のイベントループでrebuildFromModel()を実行する（連続する編集をまとめ、
  // マウスイベント処理中の自己破壊を避けるための遅延実行）
  void scheduleRebuild();
  // ノード移動などに追従してリンクのパスだけを引き直す
  void updateLinkPaths();

  WorkflowNodeItem* nodeItem(WorkflowType type) const {
    return m_nodeItems.value(type, nullptr);
  }

  // このシーン自身の編集操作によってmarkDirty()が呼ばれている間true。
  // ダイアログ側はこれを見て、シーン起因の変更では再構築をスキップする
  bool isSelfEditing() const { return m_selfEditing; }

  // rebuildFromModel()の実行中true（アイテム側が、生成時のsetPos()を
  // ユーザーによる移動と区別するために参照する）
  bool isRebuilding() const { return m_rebuilding; }

  // A→Bが1対1接続（Aの後工程はBのみ、Bへの流入はAのみ）か
  bool isOneToOne(WorkflowType from, WorkflowType to) const;
  // 1対1接続のノード同士が上下に接触して直結表示されているか（このときリンク線は省略する）
  bool isDirectlyAttached(WorkflowType from, WorkflowType to) const;

  // --- アイテム側から呼ばれる操作 ---
  void beginLinkDrag(WorkflowType from, const ActionRef& fixedAction,
                     const QPointF& startScenePos);
  bool isLinkDragging() const { return m_linkDragLine != nullptr; }
  void notifyNodeMoved();  // 移動中：リンク再描画
  void notifyNodeMoveFinished(
      WorkflowNodeItem* node);  // 移動確定：スナップ・所属変更・markDirty
  void notifyGroupMoveFinished(GroupNodeItem* group);
  void notifyNodeDoubleClicked(WorkflowType wfType);
  void notifyGroupDoubleClicked(FlowGroupId groupId);
  void toggleNodeExpanded(WorkflowNodeItem* node);
  // シーン起因の編集としてmarkDirty()を呼ぶ（m_selfEditingガード付き）
  void markSelfDirty();

  // --- 役職パレット（FlowChartView）から呼ばれる操作 ---
  // 工程グループノードに役職をドロップ：独立した新規工程ノードを役職の既定値で作成する
  void addWorkflowNodeFromRole(FlowGroupId groupId, const QPointF& scenePos,
                               const Role& role);
  // 工程ノードに役職をドロップ：ドロップ先ノードの次工程として連結した新規ノードを作成する
  void addConnectedWorkflowNode(WorkflowNodeItem* sourceItem,
                                const QString& name, const QColor& color,
                                const QString& suffix);
  WorkflowNodeItem* nodeItemAt(const QPointF& scenePos) const;
  GroupNodeItem* groupItemAt(const QPointF& scenePos) const;
  // 指定グループに役職を配置した場合の既定の工程名（"[グループ名]
  // [役職名]"）を返す
  QString composeRoleWorkflowName(FlowGroupId groupId, const Role& role) const;
  // 指定した工程ノードが所属するグループにroleを配置した場合の既定の工程名を返す
  QString composeRoleWorkflowNameForNode(WorkflowNodeItem* nodeItem,
                                         const Role& role) const;
  // 何もない場所への右クリック／ドロップ用：グループを選ぶメニューを出す（新規グループ作成も可）。
  // キャンセル時はGroup_Noneを返す
  FlowGroupId promptForGroupChoice(QWidget* parent, const QPoint& globalPos);

signals:
  void nodeDoubleClicked(WorkflowType wfType);
  void groupNodeDoubleClicked(FlowGroupId groupId);

protected:
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private:
  // 工程ノードを1つ作成し、tmpSettings（workflows／flowChartLayout）へ登録する。
  // markSelfDirty()/scheduleRebuild()は呼ばないので、呼び出し側で行うこと
  WorkflowType createWorkflowNode(FlowGroupId groupId, const QPointF& scenePos,
                                  const QString& name, const QColor& color,
                                  const QString& suffix);
  // 接続元アクションの選択（候補が1つなら自動確定、複数ならチェックボックス付きダイアログ）。
  // excludeIfAlreadyLinkedToがWf_Undefinedでなければ、既にそのWorkflowへリンク済みのアクションを候補から除く
  QList<ActionRef> selectSourceActions(QWidget* parent, Workflow* wf,
                                       WorkflowType excludeIfAlreadyLinkedTo,
                                       const QString& targetLabel);

  void completeLinkDrag(WorkflowNodeItem* target);
  void cancelLinkDrag();
  void deleteSelectedItems();
  void addWorkflowNodeAt(FlowGroupId groupId, const QPointF& scenePos);
  // 1対1接続の相手ノードが近接している場合、上下辺が接し左右が中央揃えになるようスナップする
  void trySnapNode(WorkflowNodeItem* node);
  // nodeを常にaboveNodeの直下へ接触＋中央揃えで配置する（距離のしきい値なしで無条件に行う）
  void attachNodeBelow(WorkflowNodeItem* node, WorkflowNodeItem* aboveNode);
  // レイアウトに未登録のWorkflowのノードを補完し、削除済みWorkflow/Groupのエントリを取り除く
  void normalizeLayout();
  // グループ枠のエントリを工程ノードの現在位置から導出し直す
  // （同一グループ内でリンクの繋がっている連結成分ごとに1枠。プレースホルダー枠は維持）
  void recomputeGroupNodeEntries();
  // グループ枠アイテムを導出し直したエントリに合わせて作り直す（ノードのドラッグに追従させる）
  void updateGroupGeometries();

  QMap<WorkflowType, WorkflowNodeItem*> m_nodeItems;
  QList<GroupNodeItem*> m_groupItems;
  QList<LinkItem*> m_linkItems;
  QList<WarningMarkItem*> m_warningItems;

  QGraphicsPathItem* m_linkDragLine = nullptr;
  WorkflowType m_linkDragFrom       = Wf_Undefined;
  ActionRef m_linkDragAction;  // アクションチップ起点の場合のみ有効
  QPointF m_linkDragStartPos;

  bool m_selfEditing      = false;
  bool m_rebuildScheduled = false;
  bool m_rebuilding =
      false;  // rebuildFromModel実行中（ノード生成時のsetPosによる再入を防ぐ）
};

//--------------------------------
// 役職パレット：tmpSettings.rolesを縦に並べた円形アイコン＋末尾の「＋」アイコンを自前で描画する。
// アイコンのドラッグはFlowChartViewへのドロップとして処理される（MIMEタイプRolePaletteMimeType）
class RolePaletteWidget : public QWidget {
  Q_OBJECT
public:
  RolePaletteWidget(QWidget* parent = nullptr);

  // tmpSettings.rolesの内容から表示を作り直す（役職の追加・編集・削除の度に呼ぶ）
  void refresh();
  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;

private:
  int roleIndexAt(const QPoint& pos) const;
  QRectF roleIconRect(int index) const;
  QRectF addButtonRect() const;
  void openAddRoleDialog();
  void openEditRoleDialog(int index);
  void deleteRole(int index);
  // labelName/suffix/colorをダイアログで編集する。キャンセル時はfalseを返す
  bool editRoleDialog(Role& role, const QString& title);

  int m_pressedIndex = -1;  // -1=なし, roles.size()="+"ボタン
  QPoint m_dragStartPos;
};

//--------------------------------
class FlowChartView : public QGraphicsView {
  Q_OBJECT
public:
  FlowChartView(QWidget* parent = nullptr);
  RolePaletteWidget* rolePalette() const { return m_rolePalette; }

protected:
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

private:
  void repositionRolePalette();

  bool m_panning = false;
  QPoint m_panLastPos;
  RolePaletteWidget* m_rolePalette;
};

// 役職パレットからのドラッグ＆ドロップに使うMIMEタイプ。データは役職のid（unsigned
// int）を文字列化したもの
extern const char* RolePaletteMimeType;

//--------------------------------
// レイアウト未保存のプロジェクト向けに、nextFlowsの接続からノード配置を自動生成する
FlowChartLayout buildDefaultFlowChartLayout();
