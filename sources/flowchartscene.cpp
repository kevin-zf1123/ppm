#include "flowchartscene.h"
#include "projectsettings_common.h"
#include "projectsettingsdialog.h"  // ColorButton

#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QFontMetricsF>
#include <QFontMetrics>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QTimer>
#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSet>
#include <QtMath>
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QIcon>
#include <QPixmap>
#include <algorithm>

const char* RolePaletteMimeType = "application/x-ppm-role-id";

namespace {

// ノードの寸法などの定数
const qreal NodeMinWidth        = 180;
const qreal NodeHeaderHeight    = 30;
const qreal NodeRadius          = 9;
const qreal ProtocolMargin      = 8;
const qreal ProtocolSpacing     = 6;
const qreal ProtocolLabelHeight = 20;
const qreal ChipHeight          = 20;
const qreal ChipSpacing         = 4;
const qreal LinkOutStripHeight =
    7;  // 折りたたみノード下辺の、リンクドラッグを開始できる帯の高さ
const qreal SnapDistance     = 14;  // 1対1接続の直結スナップが働く距離
const qreal GroupLabelHeight = 30;
const qreal GroupMargin =
    20;  // 自動導出されるグループ枠の、工程ノード群からのマージン

QColor neutralLinkColor() { return QColor(45, 45, 45); }

// アクションごとのリンク色。追いやすいよう、隣接インデックスで色相が離れるパレットにする
QColor actionPaletteColor(int index) {
  static const int hues[] = {340, 110, 210, 30, 275, 170, 55, 0};
  int hue                 = (hues[index % 8] + (index / 8) * 15) % 360;
  return QColor::fromHsv(hue, 130, 235);
}

QColor textColorFor(const QColor& bg) {
  return (bg.lightness() > 128) ? QColor(25, 25, 25) : QColor(240, 240, 240);
}

const Workflow* workflowByType(WorkflowType type) {
  for (auto& wf : tmpSettings.workflows)
    if (wf.workflowType == type) return &wf;
  return nullptr;
}

Workflow* mutableWorkflowByType(WorkflowType type) {
  for (auto& wf : tmpSettings.workflows)
    if (wf.workflowType == type) return &wf;
  return nullptr;
}

int workflowIndexByType(WorkflowType type) {
  for (int i = 0; i < tmpSettings.workflows.size(); i++)
    if (tmpSettings.workflows.at(i).workflowType == type) return i;
  return -1;
}

FlowChartWorkflowNode* layoutWorkflowNode(WorkflowType type) {
  for (auto& node : tmpSettings.flowChartLayout.workflowNodes)
    if (node.workflowType == type) return &node;
  return nullptr;
}

// ノードの背景色。Workflow::colorが未設定の場合は薄いグレーにする
QColor nodeBgColor(const Workflow* wf) {
  if (wf && wf->color.isValid() && wf->color.alpha() > 0 &&
      wf->color != QColor(0, 0, 0, 255))
    return wf->color;
  return QColor(232, 232, 232);
}

// 2つのノードの水平方向の重なり幅
qreal horizontalOverlap(const QRectF& a, const QRectF& b) {
  return qMin(a.right(), b.right()) - qMax(a.left(), b.left());
}

// actionの完了時に生成されるファイル（Step_Rename/Step_Copy/Step_CreateCueFile）の中に、
// targetType側のいずれかのプロトコルのCondition Pathと一致するものがあるか。
// clearsRewindがtrue（元の工程が「グループ内最後の工程」）のときは、アクションの後に
// カット情報REWINDが外れ、後工程の条件パスの[R]は空になるので、[R]を除いて比べる
bool actionMatchesTargetConditionPath(const PPM_Action& action,
                                      WorkflowType targetType,
                                      bool clearsRewind) {
  const Workflow* toWf = workflowByType(targetType);
  if (!toWf) return true;  // 対象が見つからない場合は警告を出さない（保守的）
  QStringList generated = collectGeneratedCueFilePaths(action);
  for (auto& protocol : toWf->protocols) {
    if (protocol.conditionPath.isEmpty()) continue;
    QString conditionPath = protocol.conditionPath;
    if (clearsRewind) conditionPath.remove("[R]");
    if (generated.contains(conditionPath)) return true;
  }
  return false;
}

QPixmap warningIconPixmap() {
  static QPixmap pm = QIcon(":Resources/warning.svg").pixmap(18, 18);
  return pm;
}

}  // namespace

//--------------------------------
// WorkflowNodeItem
//--------------------------------

WorkflowNodeItem::WorkflowNodeItem(FlowChartScene* chartScene,
                                   WorkflowType wfType)
    : m_chartScene(chartScene)
    , m_wfType(wfType)
    , m_size(NodeMinWidth, NodeHeaderHeight) {
  setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable |
           QGraphicsItem::ItemSendsGeometryChanges);
  // 展開中のノードは最前面に出し、内容が他の工程ノードに隠れないようにする
  setZValue(isExpanded() ? 20 : 10);
  setAcceptHoverEvents(
      true);  // Condition
              // Path未設定箇所の説明ツールチップを動的に切り替えるため
  rebuildGeometry();
}

const Workflow* WorkflowNodeItem::findWorkflow() const {
  return workflowByType(m_wfType);
}

bool WorkflowNodeItem::isExpanded() const {
  FlowChartWorkflowNode* node = layoutWorkflowNode(m_wfType);
  return node ? node->expanded : false;
}

QRectF WorkflowNodeItem::toggleRect() const {
  return QRectF(7, (NodeHeaderHeight - 14) / 2, 15, 14);
}

void WorkflowNodeItem::rebuildGeometry() {
  prepareGeometryChange();
  m_protocolBoxes.clear();
  m_actionChips.clear();

  const Workflow* wf = findWorkflow();
  if (!wf) {
    m_size                  = QSizeF(NodeMinWidth, NodeHeaderHeight);
    m_hasEmptyConditionPath = false;
    return;
  }

  // 折りたたみ時のハイライト判定は、展開状態によらず全プロトコルを対象にする
  m_hasEmptyConditionPath = false;
  for (auto& protocol : wf->protocols) {
    if (protocol.conditionPath.isEmpty()) {
      m_hasEmptyConditionPath = true;
      break;
    }
  }

  QFont font;
  QFontMetricsF fm(font);
  qreal headerW = qMax(NodeMinWidth, fm.horizontalAdvance(wf->name) + 46);

  if (!isExpanded() || wf->protocols.isEmpty()) {
    m_size = QSizeF(headerW, NodeHeaderHeight);
    return;
  }

  int chipColorIndex = 0;
  qreal x            = ProtocolMargin;
  const qreal boxTop = NodeHeaderHeight + 4;
  const qreal boxH   = ProtocolLabelHeight + ChipHeight + 8;

  for (int p = 0; p < wf->protocols.size(); p++) {
    const WorkflowProtocol& protocol = wf->protocols.at(p);

    // アクションチップの合計幅から、プロトコル箱の幅を決める
    QList<ActionChipGeom> chips;
    qreal chipsW = 0;
    for (int a = 0; a < protocol.actions.size(); a++) {
      QString label = protocol.actions.at(a).actionName;
      if (label.isEmpty()) label = QObject::tr("(unnamed)");
      ActionChipGeom chip;
      chip.label = label;
      chip.ref   = {p, a};
      chip.color = actionPaletteColor(chipColorIndex++);
      chip.rect  = QRectF(0, 0, fm.horizontalAdvance(label) + 14, ChipHeight);
      if (a > 0) chipsW += ChipSpacing;
      chipsW += chip.rect.width();
      chips.append(chip);
    }

    QString condText = protocol.conditionPath.isEmpty()
                           ? QObject::tr("(no condition path)")
                           : protocol.conditionPath;
    qreal labelW     = qMin(fm.horizontalAdvance(condText) + 16, (qreal)280);
    qreal boxW       = qMax(qMax(chipsW + 12, labelW), (qreal)110);

    ProtocolBoxGeom box;
    box.rect     = QRectF(x, boxTop, boxW, boxH);
    box.fullText = protocol.conditionPath;
    box.label    = QFontMetrics(font).elidedText(condText, Qt::ElideMiddle,
                                                 (int)boxW - 12);
    m_protocolBoxes.append(box);

    qreal cx = box.rect.left() + 6;
    qreal cy = box.rect.bottom() - ChipHeight - 3;
    for (auto& chip : chips) {
      chip.rect.moveTo(cx, cy);
      cx += chip.rect.width() + ChipSpacing;
      m_actionChips.append(chip);
    }

    x += boxW + ProtocolSpacing;
  }

  qreal totalW = qMax(headerW, x - ProtocolSpacing + ProtocolMargin);
  m_size       = QSizeF(totalW, boxTop + boxH + ProtocolMargin);
}

QRectF WorkflowNodeItem::boundingRect() const {
  return QRectF(QPointF(0, 0), m_size).adjusted(-1, -1, 1, 1);
}

void WorkflowNodeItem::paint(QPainter* painter,
                             const QStyleOptionGraphicsItem* option,
                             QWidget* widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);
  const Workflow* wf = findWorkflow();
  if (!wf) return;

  painter->setRenderHint(QPainter::Antialiasing);

  QColor bg = nodeBgColor(wf);
  QRectF rect(QPointF(0, 0), m_size);
  // 折りたたみ時：いずれかのプロトコルのCondition
  // Pathが未設定なら、枠全体を赤く強調する
  bool highlightWholeNode = !isExpanded() && m_hasEmptyConditionPath;
  if (isSelected())
    painter->setPen(QPen(QColor(30, 120, 255), 2));
  else if (highlightWholeNode)
    painter->setPen(QPen(QColor(210, 30, 30), 2.5));
  else
    painter->setPen(QPen(bg.darker(135), 1));
  painter->setBrush(bg);
  painter->drawRoundedRect(rect, NodeRadius, NodeRadius);

  QColor textColor = textColorFor(bg);

  // 展開／折りたたみトグル（▶/▼）
  painter->setPen(Qt::NoPen);
  painter->setBrush(textColor);
  QRectF tr = toggleRect();
  QPolygonF triangle;
  if (isExpanded())
    triangle << QPointF(tr.left(), tr.top() + 4)
             << QPointF(tr.right(), tr.top() + 4)
             << QPointF(tr.center().x(), tr.bottom());
  else
    triangle << QPointF(tr.left() + 3, tr.top())
             << QPointF(tr.left() + 3, tr.bottom())
             << QPointF(tr.right() - 1, tr.center().y());
  painter->drawPolygon(triangle);

  // 工程名
  painter->setPen(textColor);
  painter->drawText(QRectF(28, 0, m_size.width() - 34, NodeHeaderHeight),
                    Qt::AlignLeft | Qt::AlignVCenter, wf->name);

  // 展開時：プロトコル箱とアクションチップ
  for (auto& box : m_protocolBoxes) {
    painter->setBrush(QColor(0, 0, 0, 28));
    if (box.fullText.isEmpty()) {
      // このプロトコルのCondition Pathが未設定：枠を赤く強調する
      painter->setPen(QPen(QColor(210, 30, 30), 2));
    } else {
      painter->setPen(Qt::NoPen);
    }
    painter->drawRoundedRect(box.rect, 5, 5);
    painter->setPen(textColor);
    painter->drawText(QRectF(box.rect.left() + 6, box.rect.top() + 1,
                             box.rect.width() - 12, ProtocolLabelHeight),
                      Qt::AlignLeft | Qt::AlignVCenter, box.label);
  }
  for (auto& chip : m_actionChips) {
    painter->setPen(Qt::NoPen);
    painter->setBrush(chip.color);
    painter->drawRoundedRect(chip.rect, 5, 5);
    painter->setPen(textColorFor(chip.color));
    painter->drawText(chip.rect, Qt::AlignCenter, chip.label);
  }
}

QPointF WorkflowNodeItem::inputPortScenePos() const {
  return mapToScene(QPointF(m_size.width() / 2, 0));
}

QPointF WorkflowNodeItem::outputPortScenePos() const {
  return mapToScene(QPointF(m_size.width() / 2, m_size.height()));
}

QPointF WorkflowNodeItem::actionPortScenePos(const ActionRef& ref) const {
  for (auto& chip : m_actionChips) {
    if (chip.ref == ref)
      return mapToScene(QPointF(chip.rect.center().x(), chip.rect.bottom()));
  }
  return outputPortScenePos();
}

QColor WorkflowNodeItem::actionColor(const ActionRef& ref) const {
  for (auto& chip : m_actionChips) {
    if (chip.ref == ref) return chip.color;
  }
  return neutralLinkColor();
}

ActionRef WorkflowNodeItem::actionChipAt(const QPointF& itemPos) const {
  for (auto& chip : m_actionChips) {
    if (chip.rect.contains(itemPos)) return chip.ref;
  }
  return ActionRef();
}

void WorkflowNodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
  // どの経路のプレスでも、前回操作やシーン再構築時のsetPos()で立った移動フラグを持ち越さない
  // （持ち越すと、クリックしただけでnotifyNodeMoveFinished()のスナップ処理が誤発動する）
  m_moved = false;
  if (event->button() == Qt::LeftButton) {
    if (toggleRect().adjusted(-3, -3, 3, 3).contains(event->pos())) {
      m_chartScene->toggleNodeExpanded(this);
      event->accept();
      return;
    }
    // アクションチップ／折りたたみノードの下辺帯からはリンクドラッグを開始する
    ActionRef ref = actionChipAt(event->pos());
    if (ref.isValid()) {
      m_chartScene->beginLinkDrag(m_wfType, ref, actionPortScenePos(ref));
      event->accept();
      return;
    }
    if (!isExpanded() && QRectF(0, m_size.height() - LinkOutStripHeight,
                                m_size.width(), LinkOutStripHeight)
                             .contains(event->pos())) {
      m_chartScene->beginLinkDrag(m_wfType, ActionRef(), outputPortScenePos());
      event->accept();
      return;
    }
  }
  QGraphicsItem::mousePressEvent(event);
}

void WorkflowNodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
  QGraphicsItem::mouseReleaseEvent(event);
  if (m_moved) {
    m_chartScene->notifyNodeMoveFinished(this);
    m_moved = false;
  }
}

void WorkflowNodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
  Q_UNUSED(event);
  m_chartScene->notifyNodeDoubleClicked(m_wfType);
  event->accept();
}

void WorkflowNodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
  updateHoverTooltip(event->pos());
  QGraphicsItem::hoverEnterEvent(event);
}

void WorkflowNodeItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event) {
  updateHoverTooltip(event->pos());
  QGraphicsItem::hoverMoveEvent(event);
}

void WorkflowNodeItem::updateHoverTooltip(const QPointF& itemPos) {
  QString tip;
  if (isExpanded()) {
    for (auto& box : m_protocolBoxes) {
      if (!box.rect.contains(itemPos)) continue;
      if (box.fullText.isEmpty())
        tip = QObject::tr("This protocol has no condition path set.");
      break;
    }
  } else if (m_hasEmptyConditionPath) {
    tip = QObject::tr(
        "This workflow has a protocol with no condition path set.\nExpand it "
        "to see which one.");
  }
  if (tip != toolTip()) setToolTip(tip);
}

QVariant WorkflowNodeItem::itemChange(GraphicsItemChange change,
                                      const QVariant& value) {
  if (change == QGraphicsItem::ItemPositionHasChanged) {
    if (FlowChartWorkflowNode* node = layoutWorkflowNode(m_wfType))
      node->pos = pos();
    // シーン再構築時の初期配置のsetPos()は「ユーザーによる移動」として扱わない
    if (m_chartScene && !m_chartScene->isRebuilding()) m_moved = true;
    if (m_chartScene) m_chartScene->notifyNodeMoved();
  }
  return QGraphicsItem::itemChange(change, value);
}

//--------------------------------
// GroupNodeItem
//--------------------------------

GroupNodeItem::GroupNodeItem(FlowChartScene* chartScene, int layoutIndex)
    : m_chartScene(chartScene), m_layoutIndex(layoutIndex), m_size(360, 260) {
  // 枠の位置・サイズは工程ノードから自動導出されるため、選択対象にはしない
  // （ラバーバンド選択で工程ノードのみが選ばれるようにする）
  setFlags(QGraphicsItem::ItemIsMovable |
           QGraphicsItem::ItemSendsGeometryChanges);
  setZValue(-10);
  if (FlowChartGroupNode* node = layoutNode()) {
    if (node->size.width() >= 60 && node->size.height() >= 40)
      m_size = node->size;
    else
      node->size = m_size;
  }
}

FlowChartGroupNode* GroupNodeItem::layoutNode() const {
  if (m_layoutIndex < 0 ||
      m_layoutIndex >= tmpSettings.flowChartLayout.groupNodes.size())
    return nullptr;
  return &tmpSettings.flowChartLayout.groupNodes[m_layoutIndex];
}

FlowGroupId GroupNodeItem::groupId() const {
  FlowChartGroupNode* node = layoutNode();
  return node ? node->groupId : Group_None;
}

QRectF GroupNodeItem::boundingRect() const {
  return QRectF(QPointF(0, 0), m_size).adjusted(-1, -1, 1, 1);
}

void GroupNodeItem::paint(QPainter* painter,
                          const QStyleOptionGraphicsItem* option,
                          QWidget* widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);
  painter->setRenderHint(QPainter::Antialiasing);

  QRectF rect(QPointF(0, 0), m_size);
  painter->setPen(QPen(QColor(120, 160, 150, 120), 1));
  painter->setBrush(QColor(150, 195, 185, 90));
  painter->drawRoundedRect(rect, 14, 14);

  // グループ名
  QString label;
  FlowGroupId fgId = groupId();
  if (tmpSettings.flowGroupInfos.contains(fgId)) {
    const FlowGroupInfo& info = tmpSettings.flowGroupInfos.value(fgId);
    label = info.labelName.isEmpty() ? info.idName : info.labelName;
  }
  if (label.isEmpty()) label = QObject::tr("(unknown group)");
  QFont font;
  font.setBold(true);
  painter->setFont(font);
  painter->setPen(QColor(30, 50, 45));
  painter->drawText(QRectF(14, 4, m_size.width() - 28, GroupLabelHeight),
                    Qt::AlignLeft | Qt::AlignVCenter, label);
}

void GroupNodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
  // 枠の移動はラベル帯からのみ行う。枠の中身の領域から開始したドラッグは
  // ラバーバンド選択（工程ノードの複数選択）に譲るため、イベントを受け取らない
  if (event->pos().y() > GroupLabelHeight) {
    event->ignore();
    return;
  }

  // ドラッグ開始時に「枠内にあり、かつこのグループに所属している」工程ノードを記憶し、一緒に移動させる
  m_draggingByMouse = true;
  m_moved           = false;
  m_lastPos         = pos();
  m_draggedMembers.clear();
  QRectF sceneRect = sceneBoundingRect();
  for (auto item : scene()->items()) {
    if (WorkflowNodeItem* node = dynamic_cast<WorkflowNodeItem*>(item)) {
      if (!sceneRect.contains(node->sceneBoundingRect().center())) continue;
      const Workflow* wf = workflowByType(node->workflowType());
      if (!wf || wf->groupId != groupId()) continue;
      m_draggedMembers.append(node);
    }
  }
  QGraphicsItem::mousePressEvent(event);
}

void GroupNodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
  QGraphicsItem::mouseReleaseEvent(event);
  if (m_moved) m_chartScene->notifyGroupMoveFinished(this);
  m_draggingByMouse = false;
  m_draggedMembers.clear();
  m_moved = false;
}

void GroupNodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
  Q_UNUSED(event);
  m_chartScene->notifyGroupDoubleClicked(groupId());
  event->accept();
}

QVariant GroupNodeItem::itemChange(GraphicsItemChange change,
                                   const QVariant& value) {
  if (change == QGraphicsItem::ItemPositionHasChanged) {
    QPointF delta = pos() - m_lastPos;
    m_lastPos     = pos();
    // マウスドラッグ中のみ、枠内の工程ノードを一緒に移動させる
    // （rebuildFromModel等によるsetPos()では動かさない）
    if (m_draggingByMouse && !delta.isNull()) {
      for (auto node : m_draggedMembers) node->setPos(node->pos() + delta);
    }
    if (FlowChartGroupNode* node = layoutNode()) node->pos = pos();
    m_moved = true;
  }
  return QGraphicsItem::itemChange(change, value);
}

//--------------------------------
// LinkItem
//--------------------------------

LinkItem::LinkItem(FlowChartScene* chartScene, WorkflowType from,
                   WorkflowType to)
    : m_chartScene(chartScene)
    , m_from(from)
    , m_to(to)
    , color(neutralLinkColor()) {
  setFlag(QGraphicsItem::ItemIsSelectable);
  setZValue(0);
}

void LinkItem::updatePath() {
  WorkflowNodeItem* fromItem = m_chartScene->nodeItem(m_from);
  WorkflowNodeItem* toItem   = m_chartScene->nodeItem(m_to);
  if (!fromItem || !toItem) {
    setVisible(false);
    return;
  }
  // 1対1接続でノード同士が直結されている場合はリンク線を省略する
  if (m_chartScene->isDirectlyAttached(m_from, m_to)) {
    setVisible(false);
    return;
  }
  setVisible(true);

  QPointF start = (fromItem->isExpanded() && actionRefs.size() == 1)
                      ? fromItem->actionPortScenePos(actionRefs.first())
                      : fromItem->outputPortScenePos();
  QPointF end   = toItem->inputPortScenePos();

  qreal dy = qMax<qreal>(30, qAbs(end.y() - start.y()) * 0.4);
  QPainterPath newPath(start);
  newPath.cubicTo(start + QPointF(0, dy), end - QPointF(0, dy), end);
  setPath(newPath);
}

QPainterPath LinkItem::shape() const {
  // 細い線のままだと選択しづらいため、太めの当たり判定を返す
  if (path().isEmpty()) return QPainterPath();
  QPainterPathStroker stroker;
  stroker.setWidth(10);
  return stroker.createStroke(path());
}

void LinkItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                     QWidget* widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setPen(QPen(isSelected() ? QColor(30, 120, 255) : color,
                       isSelected() ? 3 : 2,
                       conditional ? Qt::DashLine : Qt::SolidLine));
  painter->setBrush(Qt::NoBrush);
  painter->drawPath(path());
}

//--------------------------------
// WarningMarkItem
//--------------------------------

WarningMarkItem::WarningMarkItem(LinkItem* link, const QString& tooltipText)
    : m_link(link) {
  setZValue(60);  // リンク・ノードより手前に表示する
  setToolTip(tooltipText);
}

QRectF WarningMarkItem::boundingRect() const { return QRectF(-9, -9, 18, 18); }

void WarningMarkItem::paint(QPainter* painter,
                            const QStyleOptionGraphicsItem* option,
                            QWidget* widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);
  painter->setRenderHint(QPainter::Antialiasing);
  painter->drawPixmap(boundingRect().toRect(), warningIconPixmap());
}

void WarningMarkItem::updatePosition(FlowChartScene* chartScene) {
  WorkflowNodeItem* fromItem = chartScene->nodeItem(m_link->fromType());
  WorkflowNodeItem* toItem   = chartScene->nodeItem(m_link->toType());
  if (!fromItem || !toItem) {
    setVisible(false);
    return;
  }
  setVisible(true);

  if (chartScene->isDirectlyAttached(m_link->fromType(), m_link->toType())) {
    // リンク線が非表示のため、ノード同士が接触している境界上にマークを置く
    setPos((fromItem->outputPortScenePos() + toItem->inputPortScenePos()) / 2);
    return;
  }

  if (!m_link->path().isEmpty()) {
    setPos(m_link->path().pointAtPercent(0.5));
    return;
  }

  // パス未計算時のフォールバック：接続元・接続先ポートの中点
  QPointF start = (fromItem->isExpanded() && m_link->actionRefs.size() == 1)
                      ? fromItem->actionPortScenePos(m_link->actionRefs.first())
                      : fromItem->outputPortScenePos();
  setPos((start + toItem->inputPortScenePos()) / 2);
}

//--------------------------------
// FlowChartScene
//--------------------------------

FlowChartScene::FlowChartScene(QObject* parent) : QGraphicsScene(parent) {
  setBackgroundBrush(QColor(241, 238, 232));
}

void FlowChartScene::markSelfDirty() {
  m_selfEditing = true;
  markDirty();
  m_selfEditing = false;
}

void FlowChartScene::scheduleRebuild() {
  if (m_rebuildScheduled) return;
  m_rebuildScheduled = true;
  QTimer::singleShot(0, this, [this]() {
    m_rebuildScheduled = false;
    rebuildFromModel();
  });
}

void FlowChartScene::normalizeLayout() {
  FlowChartLayout& layout = tmpSettings.flowChartLayout;

  // 削除済みWorkflow／グループを参照するエントリを取り除く
  for (int i = layout.workflowNodes.size() - 1; i >= 0; i--) {
    if (!workflowByType(layout.workflowNodes.at(i).workflowType))
      layout.workflowNodes.removeAt(i);
  }
  for (int i = layout.groupNodes.size() - 1; i >= 0; i--) {
    if (!tmpSettings.flowGroupInfos.contains(layout.groupNodes.at(i).groupId))
      layout.groupNodes.removeAt(i);
  }

  // レイアウト未登録のWorkflowのノードを、現在の配置の下端に縦積みで補完する
  qreal maxY = 40;
  for (auto& node : layout.workflowNodes) maxY = qMax(maxY, node.pos.y() + 50);
  for (auto& wf : tmpSettings.workflows) {
    if (!layoutWorkflowNode(wf.workflowType)) {
      FlowChartWorkflowNode node;
      node.workflowType = wf.workflowType;
      node.pos          = QPointF(40, maxY);
      maxY += 46;
      layout.workflowNodes.append(node);
    }
  }
}

void FlowChartScene::rebuildFromModel() {
  // ドラッグ操作の最中に自アイテムを破棄しないよう、操作が終わってから作り直す
  if (mouseGrabberItem()) {
    scheduleRebuild();
    return;
  }
  cancelLinkDrag();

  m_rebuilding = true;

  // clear()によるアイテム破棄中にdangling
  // pointerへ触れないよう、先に管理リストを空にする
  m_nodeItems.clear();
  m_groupItems.clear();
  m_linkItems.clear();
  m_warningItems.clear();
  m_linkDragLine = nullptr;
  clear();

  normalizeLayout();
  FlowChartLayout& layout = tmpSettings.flowChartLayout;

  // 工程ノードを先に作る（グループ枠の導出にノードの実サイズが必要なため）
  for (auto& node : layout.workflowNodes) {
    WorkflowNodeItem* item = new WorkflowNodeItem(this, node.workflowType);
    addItem(item);
    item->setPos(node.pos);
    m_nodeItems.insert(node.workflowType, item);
  }

  // グループ枠：工程ノードの現在位置から導出し直してから作る
  recomputeGroupNodeEntries();
  for (int i = 0; i < layout.groupNodes.size(); i++) {
    GroupNodeItem* group = new GroupNodeItem(this, i);
    addItem(group);
    group->setPos(layout.groupNodes.at(i).pos);
    m_groupItems.append(group);
  }

  // リンク（nextFlowsから導出）
  for (auto& wf : tmpSettings.workflows) {
    WorkflowNodeItem* fromItem = m_nodeItems.value(wf.workflowType, nullptr);
    if (!fromItem) continue;
    bool expanded = fromItem->isExpanded();
    QMap<WorkflowType, LinkItem*>
        aggregated;  // 折りたたみ時：(from,to)ペアで1本に集約

    for (int p = 0; p < wf.protocols.size(); p++) {
      const WorkflowProtocol& protocol = wf.protocols.at(p);
      for (int a = 0; a < protocol.actions.size(); a++) {
        // nextFlowsと、Step_AddProgressで追加される工程（条件付きのものは破線で描く）
        for (const NextFlowLink& nextLink :
             protocol.actions.at(a).nextFlowLinks()) {
          const WorkflowType nextType = nextLink.type;
          if (!m_nodeItems.contains(nextType)) continue;

          bool actionOk = actionMatchesTargetConditionPath(
              protocol.actions.at(a), nextType, wf.clearsRewindOnComplete);

          if (expanded) {
            LinkItem* link   = new LinkItem(this, wf.workflowType, nextType);
            link->actionRefs = {ActionRef{p, a}};
            link->color = fromItem->actionColor(ActionRef{p, a}).darker(150);
            link->conditional = nextLink.conditional;
            link->setToolTip(protocol.actions.at(a).actionName +
                             (nextLink.conditional
                                  ? "\n" + tr("Added by an \"Add Next "
                                              "Workflow\" step (conditional)")
                                  : QString()));
            link->warning = !actionOk;
            addItem(link);
            m_linkItems.append(link);
          } else {
            LinkItem* link = aggregated.value(nextType, nullptr);
            if (!link) {
              link              = new LinkItem(this, wf.workflowType, nextType);
              link->color       = neutralLinkColor();
              link->conditional = nextLink.conditional;
              addItem(link);
              m_linkItems.append(link);
              aggregated.insert(nextType, link);
            }
            if (!nextLink.conditional)
              link->conditional =
                  false;  // 無条件で繋がるアクションがあれば実線
            link->actionRefs.append(ActionRef{p, a});
            if (!actionOk) link->warning = true;
            // ツールチップに対応アクション名の一覧を出す
            QStringList names;
            for (auto& ref : link->actionRefs)
              names.append(wf.protocols.at(ref.protocolIdx)
                               .actions.at(ref.actionIdx)
                               .actionName);
            link->setToolTip(names.join("\n") +
                             (link->conditional
                                  ? "\n" + tr("Added by an \"Add Next "
                                              "Workflow\" step (conditional)")
                                  : QString()));
          }
        }
      }
    }
  }

  // 警告マーク：後工程のCondition Pathを生成する設定が無いリンクに付ける。
  // リンク線が直結表示で非表示になっていても見えるよう、独立したアイテムとして持つ
  for (auto link : m_linkItems) {
    if (!link->warning) continue;
    WarningMarkItem* mark = new WarningMarkItem(
        link, tr("None of the source workflow's steps "
                 "(Step_Rename/Step_Copy/Step_CreateCueFile) produce a file\n"
                 "matching the destination's condition path. This may be fine "
                 "if the file is created manually,\n"
                 "but please double-check the configuration."));
    addItem(mark);
    m_warningItems.append(mark);
  }

  m_rebuilding = false;

  updateLinkPaths();
}

void FlowChartScene::recomputeGroupNodeEntries() {
  FlowChartLayout& layout = tmpSettings.flowChartLayout;

  QList<FlowChartGroupNode> newGroupNodes;

  // 所属工程が1つも無いグループの枠（手動配置のプレースホルダー）は現在の位置のまま維持する
  QSet<FlowGroupId> groupsWithMembers;
  for (auto& wf : tmpSettings.workflows) groupsWithMembers.insert(wf.groupId);
  for (auto& groupNode : layout.groupNodes) {
    if (!groupsWithMembers.contains(groupNode.groupId))
      newGroupNodes.append(groupNode);
  }

  // 工程を持つグループ：グループ内でリンクの繋がっている工程ノードのまとまり（連結成分）ごとに
  // 「バウンディングボックス＋マージン」の枠を導出する
  for (auto fgId : tmpSettings.flowGroupInfos.keys()) {
    // このグループに所属し、ノードアイテムが存在する工程の一覧
    QList<WorkflowType> members;
    for (auto& wf : tmpSettings.workflows) {
      if (wf.groupId == fgId && m_nodeItems.contains(wf.workflowType))
        members.append(wf.workflowType);
    }
    if (members.isEmpty()) continue;

    // 同一グループ内のリンクを無向の隣接関係として構築する
    QMap<WorkflowType, QList<WorkflowType>> adjacency;
    for (auto memberType : members) {
      const Workflow* wf = workflowByType(memberType);
      if (!wf) continue;
      for (auto& protocol : wf->protocols) {
        for (auto& action : protocol.actions) {
          for (auto next : action.allNextFlows()) {
            if (!members.contains(next)) continue;
            adjacency[memberType].append(next);
            adjacency[next].append(memberType);
          }
        }
      }
    }

    // 連結成分ごとに枠を作る
    QSet<WorkflowType> visited;
    for (auto memberType : members) {
      if (visited.contains(memberType)) continue;
      QList<WorkflowType> component;
      QList<WorkflowType> queue = {memberType};
      visited.insert(memberType);
      while (!queue.isEmpty()) {
        WorkflowType current = queue.takeFirst();
        component.append(current);
        for (auto neighbor : adjacency.value(current)) {
          if (visited.contains(neighbor)) continue;
          visited.insert(neighbor);
          queue.append(neighbor);
        }
      }

      QRectF bounds;
      bool first = true;
      for (auto type : component) {
        QRectF r = m_nodeItems.value(type)->sceneBoundingRect();
        bounds   = first ? r : bounds.united(r);
        first    = false;
      }

      FlowChartGroupNode groupNode;
      groupNode.groupId = fgId;
      groupNode.pos     = bounds.topLeft() -
                      QPointF(GroupMargin, GroupMargin + GroupLabelHeight);
      groupNode.size =
          QSizeF(bounds.width() + GroupMargin * 2,
                 bounds.height() + GroupMargin * 2 + GroupLabelHeight);
      newGroupNodes.append(groupNode);
    }
  }

  layout.groupNodes = newGroupNodes;
}

void FlowChartScene::updateGroupGeometries() {
  if (m_rebuilding) return;
  // グループ枠自体をドラッグ中は作り直さない（グラバーのアイテムを破棄できないため）。
  // ドラッグ終了時のscheduleRebuild()で改めて導出される
  if (dynamic_cast<GroupNodeItem*>(mouseGrabberItem())) return;

  recomputeGroupNodeEntries();

  for (auto group : m_groupItems) {
    removeItem(group);
    delete group;
  }
  m_groupItems.clear();

  FlowChartLayout& layout = tmpSettings.flowChartLayout;
  for (int i = 0; i < layout.groupNodes.size(); i++) {
    GroupNodeItem* group = new GroupNodeItem(this, i);
    addItem(group);
    group->setPos(layout.groupNodes.at(i).pos);
    m_groupItems.append(group);
  }
}

void FlowChartScene::updateLinkPaths() {
  for (auto link : m_linkItems) link->updatePath();
  // 警告マークの位置も、リンクの表示状態（直結中か、パスの中点か）に追従させる
  for (auto mark : m_warningItems) mark->updatePosition(this);
}

bool FlowChartScene::isOneToOne(WorkflowType from, WorkflowType to) const {
  // fromの全アクションの後工程がtoのみか
  const Workflow* fromWf = workflowByType(from);
  if (!fromWf) return false;
  QSet<WorkflowType> targets;
  for (auto& protocol : fromWf->protocols)
    for (auto& action : protocol.actions)
      for (auto next : action.allNextFlows()) targets.insert(next);
  if (targets.size() != 1 || !targets.contains(to)) return false;

  // toへの流入がfromのみか
  for (auto& wf : tmpSettings.workflows) {
    if (wf.workflowType == from) continue;
    for (auto& protocol : wf.protocols)
      for (auto& action : protocol.actions)
        if (action.allNextFlows().contains(to)) return false;
  }
  return true;
}

bool FlowChartScene::isDirectlyAttached(WorkflowType from,
                                        WorkflowType to) const {
  WorkflowNodeItem* fromItem = m_nodeItems.value(from, nullptr);
  WorkflowNodeItem* toItem   = m_nodeItems.value(to, nullptr);
  if (!fromItem || !toItem) return false;
  if (fromItem->isExpanded())
    return false;  // 展開中はアクション起点の線を常に表示する
  if (!isOneToOne(from, to)) return false;

  QRectF fromRect = fromItem->sceneBoundingRect();
  QRectF toRect   = toItem->sceneBoundingRect();
  qreal gap       = toRect.top() - fromRect.bottom();
  return qAbs(gap) <= 3.0 && horizontalOverlap(fromRect, toRect) > 20;
}

void FlowChartScene::trySnapNode(WorkflowNodeItem* node) {
  WorkflowType type = node->workflowType();
  QRectF nodeRect   = node->sceneBoundingRect();

  // このノードを後工程とする1対1接続元の直下にスナップ（左右は中央揃えにする）
  for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
    if (it.key() == type) continue;
    if (!isOneToOne(it.key(), type)) continue;
    QRectF srcRect = it.value()->sceneBoundingRect();
    qreal gap      = nodeRect.top() - srcRect.bottom();
    if (qAbs(gap) <= SnapDistance &&
        horizontalOverlap(srcRect, nodeRect) > 20) {
      qreal centerX =
          it.value()->pos().x() + it.value()->nodeSize().width() / 2;
      node->setPos(centerX - node->nodeSize().width() / 2,
                   it.value()->pos().y() + it.value()->nodeSize().height());
      return;
    }
  }
  // このノードの1対1後工程の直上にスナップ（左右は中央揃えにする）
  for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
    if (it.key() == type) continue;
    if (!isOneToOne(type, it.key())) continue;
    QRectF tgtRect = it.value()->sceneBoundingRect();
    qreal gap      = tgtRect.top() - nodeRect.bottom();
    if (qAbs(gap) <= SnapDistance &&
        horizontalOverlap(tgtRect, nodeRect) > 20) {
      qreal centerX =
          it.value()->pos().x() + it.value()->nodeSize().width() / 2;
      node->setPos(centerX - node->nodeSize().width() / 2,
                   it.value()->pos().y() - node->nodeSize().height());
      return;
    }
  }
}

void FlowChartScene::notifyNodeMoved() {
  if (m_rebuilding) return;
  updateLinkPaths();
  // グループ枠はノード位置から導出されるため、ノードの移動に追従させる
  updateGroupGeometries();
}

void FlowChartScene::notifyNodeMoveFinished(WorkflowNodeItem* node) {
  // ドロップ先のグループ枠に応じて所属グループを変更する（配置＝所属の直感に合わせる）。
  // グループが変わる場合、tmpSettings.workflowsの並びも新しいグループの末尾に移動して、
  // 工程グループのリスト順でクラスタリングされた状態を保つ
  int index = workflowIndexByType(node->workflowType());
  if (index >= 0) {
    QPointF center = node->sceneBoundingRect().center();
    for (auto group : m_groupItems) {
      if (group->sceneBoundingRect().contains(center)) {
        if (tmpSettings.workflows.at(index).groupId != group->groupId())
          moveWorkflowToGroupEnd(index, group->groupId());
        break;
      }
    }
  }

  trySnapNode(node);
  markSelfDirty();
  updateLinkPaths();
  // 所属グループの変更や移動により、グループ枠の範囲・分割（連結成分）が変わる可能性があるため導出し直す
  updateGroupGeometries();
}

void FlowChartScene::notifyGroupMoveFinished(GroupNodeItem* group) {
  Q_UNUSED(group);
  markSelfDirty();
  // 枠の位置・サイズは移動後のノード位置から導出し直す
  // （この時点ではまだ枠がマウスグラバーの場合があるため、遅延実行で作り直す）
  scheduleRebuild();
}

void FlowChartScene::notifyNodeDoubleClicked(WorkflowType wfType) {
  emit nodeDoubleClicked(wfType);
}

void FlowChartScene::notifyGroupDoubleClicked(FlowGroupId groupId) {
  emit groupNodeDoubleClicked(groupId);
}

void FlowChartScene::toggleNodeExpanded(WorkflowNodeItem* node) {
  if (FlowChartWorkflowNode* layoutNode =
          layoutWorkflowNode(node->workflowType())) {
    layoutNode->expanded = !layoutNode->expanded;
    markSelfDirty();
    // 展開状態でリンクの粒度（アクション単位／集約）が変わるため作り直す。
    // マウスイベント処理中の自己破壊を避けるため遅延実行
    scheduleRebuild();
  }
}

void FlowChartScene::beginLinkDrag(WorkflowType from,
                                   const ActionRef& fixedAction,
                                   const QPointF& startScenePos) {
  cancelLinkDrag();
  m_linkDragFrom     = from;
  m_linkDragAction   = fixedAction;
  m_linkDragStartPos = startScenePos;
  m_linkDragLine =
      addPath(QPainterPath(), QPen(QColor(60, 60, 60), 2, Qt::DashLine));
  m_linkDragLine->setZValue(50);
}

void FlowChartScene::cancelLinkDrag() {
  if (m_linkDragLine) {
    removeItem(m_linkDragLine);
    delete m_linkDragLine;
    m_linkDragLine = nullptr;
  }
  m_linkDragFrom   = Wf_Undefined;
  m_linkDragAction = ActionRef();
}

void FlowChartScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
  if (m_linkDragLine) {
    QPointF start = m_linkDragStartPos;
    QPointF end   = event->scenePos();
    qreal dy      = qMax<qreal>(30, qAbs(end.y() - start.y()) * 0.4);
    QPainterPath path(start);
    path.cubicTo(start + QPointF(0, dy), end - QPointF(0, dy), end);
    m_linkDragLine->setPath(path);
    event->accept();
    return;
  }
  QGraphicsScene::mouseMoveEvent(event);
}

void FlowChartScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
  if (m_linkDragLine) {
    WorkflowNodeItem* target = nullptr;
    for (auto item : items(event->scenePos())) {
      if ((target = dynamic_cast<WorkflowNodeItem*>(item))) break;
    }
    if (target && target->workflowType() != m_linkDragFrom)
      completeLinkDrag(target);
    cancelLinkDrag();
    // リンクドラッグ開始時にノードがマウスグラブを得ているため、
    // 基底実装にもreleaseを渡してグラブを確実に解除する
    QGraphicsScene::mouseReleaseEvent(event);
    return;
  }
  QGraphicsScene::mouseReleaseEvent(event);
}

QList<ActionRef> FlowChartScene::selectSourceActions(
    QWidget* parent, Workflow* wf, WorkflowType excludeIfAlreadyLinkedTo,
    const QString& targetLabel) {
  struct Candidate {
    ActionRef ref;
    QString label;
  };
  QList<Candidate> candidates;
  for (int p = 0; p < wf->protocols.size(); p++) {
    for (int a = 0; a < wf->protocols.at(p).actions.size(); a++) {
      if (excludeIfAlreadyLinkedTo != Wf_Undefined &&
          wf->protocols.at(p).actions.at(a).allNextFlows().contains(
              excludeIfAlreadyLinkedTo))
        continue;
      QString actionName = wf->protocols.at(p).actions.at(a).actionName;
      if (actionName.isEmpty()) actionName = tr("(unnamed action)");
      QString label = (wf->protocols.size() > 1)
                          ? tr("Protocol %1 : %2").arg(p + 1).arg(actionName)
                          : actionName;
      candidates.append({ActionRef{p, a}, label});
    }
  }

  if (candidates.isEmpty()) {
    QMessageBox::information(
        parent, tr("Link"),
        tr("All actions of \"%1\" already link to the selected workflow.")
            .arg(wf->name));
    return {};
  }

  if (candidates.size() == 1) {
    // アクションが1つだけなら自動で確定する
    return {candidates.first().ref};
  }

  // 複数の場合はチェックボックス付きリストで選択（複数アクション選択可）
  QDialog dialog(parent);
  dialog.setWindowTitle(tr("Select Source Action(s)"));
  QVBoxLayout* lay = new QVBoxLayout(&dialog);
  lay->addWidget(
      new QLabel(tr("Select which action(s) of \"%1\"\nlead to \"%2\" :")
                     .arg(wf->name, targetLabel)));
  QListWidget* list = new QListWidget(&dialog);
  for (int i = 0; i < candidates.size(); i++) {
    QListWidgetItem* item = new QListWidgetItem(candidates.at(i).label, list);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(i == 0 ? Qt::Checked : Qt::Unchecked);
  }
  lay->addWidget(list);
  QHBoxLayout* buttonLay    = new QHBoxLayout();
  QPushButton* okButton     = new QPushButton(tr("OK"), &dialog);
  QPushButton* cancelButton = new QPushButton(tr("Cancel"), &dialog);
  okButton->setDefault(true);
  buttonLay->addStretch(1);
  buttonLay->addWidget(okButton);
  buttonLay->addWidget(cancelButton);
  lay->addLayout(buttonLay);
  connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
  connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted) return {};
  QList<ActionRef> chosen;
  for (int i = 0; i < candidates.size(); i++) {
    if (list->item(i)->checkState() == Qt::Checked)
      chosen.append(candidates.at(i).ref);
  }
  return chosen;
}

void FlowChartScene::completeLinkDrag(WorkflowNodeItem* target) {
  Workflow* wf = mutableWorkflowByType(m_linkDragFrom);
  if (!wf) return;
  WorkflowType targetType = target->workflowType();
  QWidget* parentWidget   = views().isEmpty() ? nullptr : views().first();

  QList<ActionRef> chosen;
  if (m_linkDragAction.isValid()) {
    // アクションチップ起点：そのアクションに限定
    bool available =
        m_linkDragAction.protocolIdx < wf->protocols.size() &&
        m_linkDragAction.actionIdx <
            wf->protocols.at(m_linkDragAction.protocolIdx).actions.size() &&
        !wf->protocols.at(m_linkDragAction.protocolIdx)
             .actions.at(m_linkDragAction.actionIdx)
             .allNextFlows()
             .contains(targetType);
    if (!available) {
      QMessageBox::information(
          parentWidget, tr("Link"),
          tr("This action already links to the selected workflow."));
      return;
    }
    chosen.append(m_linkDragAction);
  } else {
    QString targetLabel = workflowByType(targetType)
                              ? workflowByType(targetType)->name
                              : QString();
    chosen = selectSourceActions(parentWidget, wf, targetType, targetLabel);
    if (chosen.isEmpty()) return;
  }

  for (auto& ref : chosen)
    wf->protocols[ref.protocolIdx].actions[ref.actionIdx].nextFlows.append(
        targetType);

  markSelfDirty();
  scheduleRebuild();
}

void FlowChartScene::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    deleteSelectedItems();
    event->accept();
    return;
  }
  QGraphicsScene::keyPressEvent(event);
}

void FlowChartScene::deleteSelectedItems() {
  QList<LinkItem*> links;
  QList<WorkflowNodeItem*> nodes;
  for (auto item : selectedItems()) {
    if (auto link = dynamic_cast<LinkItem*>(item))
      links.append(link);
    else if (auto node = dynamic_cast<WorkflowNodeItem*>(item))
      nodes.append(node);
  }
  if (links.isEmpty() && nodes.isEmpty()) return;

  QWidget* parentWidget = views().isEmpty() ? nullptr : views().first();

  QStringList messages;
  if (!links.isEmpty())
    messages.append(tr("%1 link(s) will be removed (their destinations are "
                       "removed from the source actions' \"Next Flows\").")
                        .arg(links.size()));
  // 条件分岐の中のStep_AddProgressで繋がっているリンクがあれば、そのステップも取り除かれることを伝える
  {
    bool hasStepLink = false;
    for (auto link : links) {
      const Workflow* fromWf = workflowByType(link->fromType());
      if (!fromWf) continue;
      for (auto& ref : link->actionRefs) {
        if (ref.protocolIdx < fromWf->protocols.size() &&
            ref.actionIdx <
                fromWf->protocols.at(ref.protocolIdx).actions.size() &&
            fromWf->protocols.at(ref.protocolIdx)
                .actions.at(ref.actionIdx)
                .stepAddedFlows()
                .contains(link->toType()))
          hasStepLink = true;
      }
    }
    if (hasStepLink)
      messages.append(
          tr("The \"Add Next Workflow\" step(s) that add the destinations will "
             "also "
             "be removed (set to None in conditional branches)."));
  }
  if (!nodes.isEmpty())
    messages.append(
        tr("%1 workflow(s) will be deleted together with their node(s).")
            .arg(nodes.size()));

  if (QMessageBox::question(parentWidget, tr("Delete"), messages.join("\n"),
                            QMessageBox::Ok | QMessageBox::Cancel,
                            QMessageBox::Ok) != QMessageBox::Ok)
    return;

  // リンク：対応アクションのnextFlowsから接続先を取り除く
  for (auto link : links) {
    Workflow* wf = mutableWorkflowByType(link->fromType());
    if (!wf) continue;
    for (auto& ref : link->actionRefs) {
      if (ref.protocolIdx < wf->protocols.size() &&
          ref.actionIdx < wf->protocols.at(ref.protocolIdx).actions.size()) {
        wf->protocols[ref.protocolIdx]
            .actions[ref.actionIdx]
            .nextFlows.removeAll(link->toType());
        wf->protocols[ref.protocolIdx]
            .actions[ref.actionIdx]
            .removeAddProgressSteps(link->toType());
      }
    }
  }

  // 工程ノード：Workflow本体を削除し、他工程からの参照も掃除する
  int totalNextFlowsRemoved     = 0;
  int totalReleaseProgressReset = 0;
  for (auto node : nodes) {
    WorkflowType type = node->workflowType();
    WorkflowReferenceCleanupResult result =
        cleanupWorkflowReferencesInTmpSettings(type);
    totalNextFlowsRemoved += result.nextFlowsCount;
    totalReleaseProgressReset += result.releaseProgressCount;
    for (int i = tmpSettings.workflows.size() - 1; i >= 0; i--)
      if (tmpSettings.workflows.at(i).workflowType == type)
        tmpSettings.workflows.removeAt(i);
    for (int i = tmpSettings.flowChartLayout.workflowNodes.size() - 1; i >= 0;
         i--)
      if (tmpSettings.flowChartLayout.workflowNodes.at(i).workflowType == type)
        tmpSettings.flowChartLayout.workflowNodes.removeAt(i);
  }
  if (totalNextFlowsRemoved > 0 || totalReleaseProgressReset > 0) {
    QStringList refMessages;
    if (totalNextFlowsRemoved > 0)
      refMessages.append(tr("%1 reference(s) in \"Next Flows\" were removed.")
                             .arg(totalNextFlowsRemoved));
    if (totalReleaseProgressReset > 0)
      refMessages.append(
          tr("%1 \"Release Progress\" / \"Add Next Workflow\" step(s) "
             "referencing the deleted workflow(s) were reset.")
              .arg(totalReleaseProgressReset));
    QMessageBox::information(parentWidget, tr("References Removed"),
                             refMessages.join("\n"));
  }

  markSelfDirty();
  scheduleRebuild();
}

void FlowChartScene::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
  WorkflowNodeItem* nodeItem = nullptr;
  GroupNodeItem* groupItem   = nullptr;
  for (auto item : items(event->scenePos())) {
    if (!nodeItem && (nodeItem = dynamic_cast<WorkflowNodeItem*>(item)))
      continue;
    if (!groupItem && (groupItem = dynamic_cast<GroupNodeItem*>(item)))
      continue;
  }

  QMenu menu;
  if (nodeItem) {
    QAction* toggleAction =
        menu.addAction(nodeItem->isExpanded() ? tr("Collapse") : tr("Expand"));
    menu.addSeparator();
    QAction* addNextAction = menu.addAction(tr("Add Next Workflow Node"));
    menu.addSeparator();
    QAction* autoConfigureAction = menu.addAction(
        tr("Auto-configure renaming chains for condition paths"));
    menu.addSeparator();
    QAction* deleteAction = menu.addAction(tr("Delete Workflow"));

    QAction* chosen = menu.exec(event->screenPos());
    if (chosen == toggleAction) {
      toggleNodeExpanded(nodeItem);
    } else if (chosen == addNextAction) {
      addConnectedWorkflowNode(nodeItem, tr("New Workflow"), QColor(Qt::white),
                               QString());
    } else if (chosen == autoConfigureAction) {
      QWidget* parentWidget = views().isEmpty() ? nullptr : views().first();
      autoConfigureRenamingChains(parentWidget, nodeItem->workflowType());
      markSelfDirty();
      scheduleRebuild();
    } else if (chosen == deleteAction) {
      clearSelection();
      nodeItem->setSelected(true);
      deleteSelectedItems();
    }
  } else if (groupItem) {
    QAction* addWfAction = menu.addAction(tr("Add Workflow Node Here"));

    // 枠の位置・サイズは所属工程ノードから自動導出されるため、削除できるのは
    // 所属工程を1つも持たないプレースホルダー枠のみ
    bool groupHasWorkflows = false;
    for (auto& wf : tmpSettings.workflows) {
      if (wf.groupId == groupItem->groupId()) {
        groupHasWorkflows = true;
        break;
      }
    }
    QAction* deleteAction = nullptr;
    if (!groupHasWorkflows) {
      menu.addSeparator();
      deleteAction = menu.addAction(tr("Delete Group Frame"));
    }

    QAction* chosen = menu.exec(event->screenPos());
    if (chosen == addWfAction) {
      addWorkflowNodeAt(groupItem->groupId(), event->scenePos());
    } else if (deleteAction && chosen == deleteAction) {
      int index = groupItem->layoutIndex();
      if (index >= 0 && index < tmpSettings.flowChartLayout.groupNodes.size())
        tmpSettings.flowChartLayout.groupNodes.removeAt(index);
      markSelfDirty();
      scheduleRebuild();
    }
  } else {
    // 何もないところへの右クリック：グループを選び、そのグループに所属する工程ノードを新規作成する。
    // 工程グループノード自体は所属工程ノードから自動導出される見た目でしかないため、
    // 単独では作成できない（工程ノードを含まないグループノードは存在し得ない）
    QWidget* parentWidget = views().isEmpty() ? nullptr : views().first();
    FlowGroupId chosenGroup =
        promptForGroupChoice(parentWidget, event->screenPos());
    if (chosenGroup != Group_None)
      addWorkflowNodeAt(chosenGroup, event->scenePos());
  }
  event->accept();
}

WorkflowType FlowChartScene::createWorkflowNode(FlowGroupId groupId,
                                                const QPointF& scenePos,
                                                const QString& name,
                                                const QColor& color,
                                                const QString& suffix) {
  // IDはユーザーが考える必要のない内部管理用の値として、空いている番号を自動採番する
  unsigned int newId = 0;
  for (auto& wf : tmpSettings.workflows)
    newId = qMax(newId, (unsigned int)wf.workflowType + 1);

  Workflow wf;
  wf.workflowType = (WorkflowType)newId;
  wf.name         = name;
  wf.color        = color;
  wf.suffix       = suffix;
  wf.groupId      = groupId;

  // 先にチャートを組み、リンク時に繋げるプロトコル／アクションが必ず存在するよう、
  // 空のアクションを1つだけ持ったプロトコルを1つ作った状態で追加する
  WorkflowProtocol protocol;
  protocol.mediaType     = Digital;
  protocol.conditionPath = "";
  PPM_Action action;
  action.actionName = tr("New Action");
  protocol.actions.append(action);
  wf.protocols.append(protocol);

  // 工程グループのリスト順でクラスタリングされた状態を保つため、末尾への単純追加ではなく
  // 所属グループのブロック末尾に挿入する
  tmpSettings.workflows.insert(insertPositionForWorkflowGroupEnd(groupId), wf);

  FlowChartWorkflowNode node;
  node.workflowType = wf.workflowType;
  node.pos          = scenePos;
  tmpSettings.flowChartLayout.workflowNodes.append(node);

  return wf.workflowType;
}

QString FlowChartScene::composeRoleWorkflowName(FlowGroupId groupId,
                                                const Role& role) const {
  QString groupLabel;
  if (tmpSettings.flowGroupInfos.contains(groupId)) {
    const FlowGroupInfo& info = tmpSettings.flowGroupInfos.value(groupId);
    groupLabel = info.labelName.isEmpty() ? info.idName : info.labelName;
  }
  return groupLabel.isEmpty() ? role.labelName
                              : tr("%1 %2").arg(groupLabel, role.labelName);
}

QString FlowChartScene::composeRoleWorkflowNameForNode(
    WorkflowNodeItem* nodeItem, const Role& role) const {
  const Workflow* wf = workflowByType(nodeItem->workflowType());
  return composeRoleWorkflowName(wf ? wf->groupId : Group_None, role);
}

void FlowChartScene::addWorkflowNodeAt(FlowGroupId groupId,
                                       const QPointF& scenePos) {
  createWorkflowNode(groupId, scenePos, tr("New Workflow"), QColor(Qt::white),
                     QString());
  markSelfDirty();
  scheduleRebuild();
}

void FlowChartScene::addWorkflowNodeFromRole(FlowGroupId groupId,
                                             const QPointF& scenePos,
                                             const Role& role) {
  createWorkflowNode(groupId, scenePos, composeRoleWorkflowName(groupId, role),
                     role.color, role.suffix);
  markSelfDirty();
  scheduleRebuild();
}

void FlowChartScene::addConnectedWorkflowNode(WorkflowNodeItem* sourceItem,
                                              const QString& name,
                                              const QColor& color,
                                              const QString& suffix) {
  WorkflowType sourceType = sourceItem->workflowType();
  Workflow* sourceWf      = mutableWorkflowByType(sourceType);
  if (!sourceWf) return;

  QWidget* parentWidget = views().isEmpty() ? nullptr : views().first();
  QList<ActionRef> chosen =
      selectSourceActions(parentWidget, sourceWf, Wf_Undefined, name);
  if (chosen.isEmpty()) return;

  FlowGroupId groupId = sourceWf->groupId;
  // 仮の配置。実際の位置は再構築後にattachNodeBelow()で確定する
  QPointF pos =
      sourceItem->pos() + QPointF(0, sourceItem->nodeSize().height() + 40);
  WorkflowType newType = createWorkflowNode(groupId, pos, name, color, suffix);

  // createWorkflowNode()内のtmpSettings.workflows.insert()でQListが再確保され得るため、
  // sourceWfポインタは使い回さず取得し直してからnextFlowsを追記する
  if (Workflow* sourceWfAfter = mutableWorkflowByType(sourceType)) {
    for (auto& ref : chosen)
      sourceWfAfter->protocols[ref.protocolIdx]
          .actions[ref.actionIdx]
          .nextFlows.append(newType);
  }

  markSelfDirty();
  scheduleRebuild();
  // 新規ノードは、常に接続元ノードの直下へ接触＋中央揃えで配置する（手動ドラッグ時のスナップ距離
  // しきい値は無関係。isOneToOneが成立する通常のケースでは、これによりリンク線も自動的に省略される）
  QTimer::singleShot(0, this, [this, newType, sourceType]() {
    WorkflowNodeItem* item      = nodeItem(newType);
    WorkflowNodeItem* aboveItem = nodeItem(sourceType);
    if (item && aboveItem) attachNodeBelow(item, aboveItem);
    updateLinkPaths();
    updateGroupGeometries();
  });
}

WorkflowNodeItem* FlowChartScene::nodeItemAt(const QPointF& scenePos) const {
  for (auto item : items(scenePos)) {
    if (auto node = dynamic_cast<WorkflowNodeItem*>(item)) return node;
  }
  return nullptr;
}

GroupNodeItem* FlowChartScene::groupItemAt(const QPointF& scenePos) const {
  for (auto item : items(scenePos)) {
    if (auto group = dynamic_cast<GroupNodeItem*>(item)) return group;
  }
  return nullptr;
}

FlowGroupId FlowChartScene::promptForGroupChoice(QWidget* parent,
                                                 const QPoint& globalPos) {
  QMenu menu;
  QMap<QAction*, FlowGroupId> groupActions;
  for (auto fgId : tmpSettings.flowGroupInfos.keys()) {
    const FlowGroupInfo& info = tmpSettings.flowGroupInfos.value(fgId);
    QString label = info.labelName.isEmpty() ? info.idName : info.labelName;
    QAction* action =
        menu.addAction(QString("%1 (%2)").arg(label, info.idName));
    groupActions.insert(action, fgId);
  }
  if (!tmpSettings.flowGroupInfos.isEmpty()) menu.addSeparator();
  QAction* newGroupAction = menu.addAction(tr("New Group..."));

  QAction* chosen = menu.exec(globalPos);
  if (!chosen) return Group_None;

  if (chosen == newGroupAction) {
    QString idName = QInputDialog::getText(
        parent, tr("New Group"), tr("ID Name of the new workflow group:"));
    if (idName.trimmed().isEmpty()) return Group_None;

    // グループIDは空いている番号を自動採番する
    FlowGroupId newFgId = 0;
    for (auto fgId : tmpSettings.flowGroupInfos.keys())
      newFgId = qMax(newFgId, fgId + 1);
    FlowGroupInfo info;
    info.idName    = idName.trimmed();
    info.labelName = idName.trimmed();
    tmpSettings.flowGroupInfos.insert(newFgId, info);
    return newFgId;
  }

  return groupActions.value(chosen, Group_None);
}

void FlowChartScene::attachNodeBelow(WorkflowNodeItem* node,
                                     WorkflowNodeItem* aboveNode) {
  qreal centerX = aboveNode->pos().x() + aboveNode->nodeSize().width() / 2;
  node->setPos(centerX - node->nodeSize().width() / 2,
               aboveNode->pos().y() + aboveNode->nodeSize().height());
}

//--------------------------------
// RolePaletteWidget
//--------------------------------

namespace {
const qreal RoleIconDiameter  = 36;
const qreal RoleIconSpacing   = 6;
const qreal RolePaletteMargin = 8;

void paintRoleIcon(QPainter* painter, const QRectF& rect, const QColor& color,
                   const QString& suffix) {
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setPen(QPen(color.darker(140), 1));
  painter->setBrush(color);
  painter->drawEllipse(rect);
  if (!suffix.isEmpty()) {
    QFont font;
    font.setPointSizeF(8.5);
    painter->setFont(font);
    painter->setPen(textColorFor(color));
    painter->drawText(rect, Qt::AlignCenter, suffix);
  }
}
}  // namespace

RolePaletteWidget::RolePaletteWidget(QWidget* parent) : QWidget(parent) {
  setMouseTracking(false);
  setToolTip(
      tr("Drag a role onto a group frame or workflow node to quickly create a "
         "workflow.\n"
         "Right-click a role to edit or delete it."));
}

QSize RolePaletteWidget::sizeHint() const {
  int count = tmpSettings.roles.size() + 1;  // +1 = "+"ボタン
  qreal h   = RolePaletteMargin * 2 + count * RoleIconDiameter +
            (count - 1) * RoleIconSpacing;
  qreal w = RolePaletteMargin * 2 + RoleIconDiameter;
  return QSize((int)w, (int)h);
}

QRectF RolePaletteWidget::roleIconRect(int index) const {
  qreal y = RolePaletteMargin + index * (RoleIconDiameter + RoleIconSpacing);
  return QRectF(RolePaletteMargin, y, RoleIconDiameter, RoleIconDiameter);
}

QRectF RolePaletteWidget::addButtonRect() const {
  return roleIconRect(tmpSettings.roles.size());
}

int RolePaletteWidget::roleIndexAt(const QPoint& pos) const {
  for (int i = 0; i < tmpSettings.roles.size(); i++) {
    if (roleIconRect(i).contains(pos)) return i;
  }
  return -1;
}

void RolePaletteWidget::refresh() {
  updateGeometry();
  // 役職の増減で高さが変わるため、右下アンカーを保ったまま位置・サイズを更新する
  QSize sz = sizeHint();
  if (parentWidget()) {
    const int margin = 12;  // FlowChartView::repositionRolePalette()と同じ値
    setGeometry(parentWidget()->width() - sz.width() - margin,
                parentWidget()->height() - sz.height() - margin, sz.width(),
                sz.height());
  } else {
    resize(sz);
  }
  update();
}

void RolePaletteWidget::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  QPainter painter(this);
  for (int i = 0; i < tmpSettings.roles.size(); i++)
    paintRoleIcon(&painter, roleIconRect(i), tmpSettings.roles.at(i).color,
                  tmpSettings.roles.at(i).suffix);

  // 「＋」ボタン
  QRectF addRect = addButtonRect();
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(QColor(120, 120, 120), 1));
  painter.setBrush(QColor(235, 235, 235));
  painter.drawEllipse(addRect);
  painter.setPen(QColor(90, 90, 90));
  QFont font;
  font.setBold(true);
  font.setPointSizeF(13);
  painter.setFont(font);
  painter.drawText(addRect, Qt::AlignCenter, "+");
}

void RolePaletteWidget::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  if (addButtonRect().contains(event->pos())) {
    openAddRoleDialog();
    return;
  }
  int index = roleIndexAt(event->pos());
  if (index < 0) return;
  m_pressedIndex = index;
  m_dragStartPos = event->pos();
}

void RolePaletteWidget::mouseMoveEvent(QMouseEvent* event) {
  if (m_pressedIndex < 0 || !(event->buttons() & Qt::LeftButton)) return;
  if ((event->pos() - m_dragStartPos).manhattanLength() <
      QApplication::startDragDistance())
    return;
  if (m_pressedIndex >= tmpSettings.roles.size()) {
    m_pressedIndex = -1;
    return;
  }
  const Role& role = tmpSettings.roles.at(m_pressedIndex);
  m_pressedIndex   = -1;

  QDrag* drag     = new QDrag(this);
  QMimeData* mime = new QMimeData();
  mime->setData(RolePaletteMimeType, QByteArray::number(role.id));
  drag->setMimeData(mime);

  QPixmap pixmap((int)RoleIconDiameter, (int)RoleIconDiameter);
  pixmap.fill(Qt::transparent);
  QPainter pixPainter(&pixmap);
  paintRoleIcon(&pixPainter, QRectF(0, 0, RoleIconDiameter, RoleIconDiameter),
                role.color, role.suffix);
  pixPainter.end();
  drag->setPixmap(pixmap);
  drag->setHotSpot(
      QPoint((int)RoleIconDiameter / 2, (int)RoleIconDiameter / 2));

  drag->exec(Qt::CopyAction);
}

void RolePaletteWidget::contextMenuEvent(QContextMenuEvent* event) {
  int index = roleIndexAt(event->pos());
  if (index < 0) return;

  QMenu menu;
  QAction* editAction   = menu.addAction(tr("Edit Role..."));
  QAction* deleteAction = menu.addAction(tr("Delete Role"));
  QAction* chosen       = menu.exec(event->globalPos());
  if (chosen == editAction)
    openEditRoleDialog(index);
  else if (chosen == deleteAction)
    deleteRole(index);
}

bool RolePaletteWidget::editRoleDialog(Role& role, const QString& title) {
  QDialog dialog(this);
  dialog.setWindowTitle(title);
  QFormLayout* formLay = new QFormLayout(&dialog);

  QLineEdit* labelEdit = new QLineEdit(role.labelName, &dialog);
  formLay->addRow(tr("Label Name"), labelEdit);

  QLineEdit* suffixEdit = new QLineEdit(role.suffix, &dialog);
  formLay->addRow(tr("Suffix"), suffixEdit);

  ColorButton* colorButton = new ColorButton(&dialog);
  colorButton->setColor(role.color.isValid() ? role.color
                                             : QColor(200, 200, 200));
  formLay->addRow(tr("Color"), colorButton);

  QHBoxLayout* buttonLay    = new QHBoxLayout();
  QPushButton* okButton     = new QPushButton(tr("OK"), &dialog);
  QPushButton* cancelButton = new QPushButton(tr("Cancel"), &dialog);
  okButton->setDefault(true);
  buttonLay->addStretch(1);
  buttonLay->addWidget(okButton);
  buttonLay->addWidget(cancelButton);
  formLay->addRow(buttonLay);
  connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
  connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted) return false;
  role.labelName = labelEdit->text();
  role.suffix    = suffixEdit->text();
  role.color     = colorButton->color();
  return true;
}

void RolePaletteWidget::openAddRoleDialog() {
  Role role;
  role.color = QColor(200, 200, 200);
  if (!editRoleDialog(role, tr("Add Role"))) return;

  unsigned int newId = 0;
  for (auto& r : tmpSettings.roles) newId = qMax(newId, r.id + 1);
  role.id = newId;

  tmpSettings.roles.append(role);
  markDirty();
  refresh();
}

void RolePaletteWidget::openEditRoleDialog(int index) {
  if (index < 0 || index >= tmpSettings.roles.size()) return;
  Role role = tmpSettings.roles.at(index);
  if (!editRoleDialog(role, tr("Edit Role"))) return;

  tmpSettings.roles[index].labelName = role.labelName;
  tmpSettings.roles[index].suffix    = role.suffix;
  tmpSettings.roles[index].color     = role.color;
  markDirty();
  refresh();
}

void RolePaletteWidget::deleteRole(int index) {
  if (index < 0 || index >= tmpSettings.roles.size()) return;
  if (QMessageBox::question(this, tr("Delete Role"), tr("Delete this role?"),
                            QMessageBox::Yes | QMessageBox::No,
                            QMessageBox::No) != QMessageBox::Yes)
    return;

  tmpSettings.roles.removeAt(index);
  markDirty();
  refresh();
}

//--------------------------------
// FlowChartView
//--------------------------------

FlowChartView::FlowChartView(QWidget* parent) : QGraphicsView(parent) {
  setRenderHint(QPainter::Antialiasing);
  setRenderHint(QPainter::TextAntialiasing);
  setDragMode(QGraphicsView::RubberBandDrag);
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  setResizeAnchor(QGraphicsView::AnchorViewCenter);
  setAcceptDrops(true);

  m_rolePalette = new RolePaletteWidget(this);
  m_rolePalette->refresh();
}

void FlowChartView::wheelEvent(QWheelEvent* event) {
  // ホイールでズーム（カーソル位置を基準にする）
  qreal factor       = qPow(1.0015, event->angleDelta().y());
  qreal currentScale = transform().m11();
  qreal newScale     = currentScale * factor;
  if (newScale < 0.1 || newScale > 4.0) {
    event->accept();
    return;
  }
  scale(factor, factor);
  event->accept();
}

void FlowChartView::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::MiddleButton) {
    m_panning    = true;
    m_panLastPos = event->pos();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }
  QGraphicsView::mousePressEvent(event);
}

void FlowChartView::mouseMoveEvent(QMouseEvent* event) {
  if (m_panning) {
    QPoint delta = event->pos() - m_panLastPos;
    m_panLastPos = event->pos();
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    event->accept();
    return;
  }
  QGraphicsView::mouseMoveEvent(event);
}

void FlowChartView::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::MiddleButton && m_panning) {
    m_panning = false;
    unsetCursor();
    event->accept();
    return;
  }
  QGraphicsView::mouseReleaseEvent(event);
}

void FlowChartView::repositionRolePalette() {
  if (!m_rolePalette) return;
  QSize sz   = m_rolePalette->sizeHint();
  int margin = 12;
  m_rolePalette->setGeometry(width() - sz.width() - margin,
                             height() - sz.height() - margin, sz.width(),
                             sz.height());
  m_rolePalette->raise();
}

void FlowChartView::resizeEvent(QResizeEvent* event) {
  QGraphicsView::resizeEvent(event);
  repositionRolePalette();
}

void FlowChartView::dragEnterEvent(QDragEnterEvent* event) {
  if (event->mimeData()->hasFormat(RolePaletteMimeType)) {
    event->acceptProposedAction();
    return;
  }
  QGraphicsView::dragEnterEvent(event);
}

void FlowChartView::dragMoveEvent(QDragMoveEvent* event) {
  if (event->mimeData()->hasFormat(RolePaletteMimeType)) {
    event->acceptProposedAction();
    return;
  }
  QGraphicsView::dragMoveEvent(event);
}

void FlowChartView::dropEvent(QDropEvent* event) {
  if (!event->mimeData()->hasFormat(RolePaletteMimeType)) {
    QGraphicsView::dropEvent(event);
    return;
  }

  unsigned int roleId = event->mimeData()->data(RolePaletteMimeType).toUInt();
  const Role* role    = nullptr;
  for (auto& r : tmpSettings.roles) {
    if (r.id == roleId) {
      role = &r;
      break;
    }
  }
  if (!role) {
    event->ignore();
    return;
  }

  FlowChartScene* chartScene = qobject_cast<FlowChartScene*>(scene());
  if (!chartScene) {
    event->ignore();
    return;
  }

  QPointF scenePos = mapToScene(event->position().toPoint());
  if (WorkflowNodeItem* nodeItem = chartScene->nodeItemAt(scenePos)) {
    chartScene->addConnectedWorkflowNode(
        nodeItem, chartScene->composeRoleWorkflowNameForNode(nodeItem, *role),
        role->color, role->suffix);
    event->acceptProposedAction();
  } else if (GroupNodeItem* groupItem = chartScene->groupItemAt(scenePos)) {
    chartScene->addWorkflowNodeFromRole(groupItem->groupId(), scenePos, *role);
    event->acceptProposedAction();
  } else {
    // 何もない場所へのドロップ：グループを選び、そのグループに所属する工程ノードを役職の既定値で作成する
    QPoint globalPos        = mapToGlobal(event->position().toPoint());
    FlowGroupId chosenGroup = chartScene->promptForGroupChoice(this, globalPos);
    if (chosenGroup != Group_None) {
      chartScene->addWorkflowNodeFromRole(chosenGroup, scenePos, *role);
      event->acceptProposedAction();
    } else {
      event->ignore();
    }
  }
}

//--------------------------------
// 自動レイアウト
//--------------------------------

FlowChartLayout buildDefaultFlowChartLayout() {
  FlowChartLayout layout;

  const qreal nodeW       = 200;
  const qreal rowH        = 70;
  const qreal colW        = 320;
  const qreal groupLabelH = 34;

  // nextFlowsの接続から各工程の深さ（レイヤー）を求める。
  // 循環があっても止まるよう、反復回数を工程数で打ち切る緩和法を使う
  QMap<WorkflowType, int> depth;
  for (auto& wf : tmpSettings.workflows) depth.insert(wf.workflowType, 0);
  int n = tmpSettings.workflows.size();
  for (int iter = 0; iter < n; iter++) {
    bool changed = false;
    for (auto& wf : tmpSettings.workflows) {
      int d = depth.value(wf.workflowType, 0);
      for (auto& protocol : wf.protocols) {
        for (auto& action : protocol.actions) {
          for (auto next : action.allNextFlows()) {
            if (!depth.contains(next)) continue;
            if (depth.value(next) < d + 1 && d + 1 <= n) {
              depth.insert(next, d + 1);
              changed = true;
            }
          }
        }
      }
    }
    if (!changed) break;
  }

  // グループごとに左から列を割り当てる
  QList<FlowGroupId> groupOrder = tmpSettings.flowGroupInfos.keys();
  QMap<FlowGroupId, int> columnIndex;
  for (int i = 0; i < groupOrder.size(); i++)
    columnIndex.insert(groupOrder.at(i), i);

  // 同一グループ・同一深さのノードの重なりを避けるためのカウンタ
  QMap<QPair<FlowGroupId, int>, int> stackCount;

  for (auto& wf : tmpSettings.workflows) {
    int column = columnIndex.value(wf.groupId,
                                   groupOrder.size());  // 未知グループは右端へ
    int d      = depth.value(wf.workflowType, 0);
    int stack  = stackCount.value({wf.groupId, d}, 0);
    stackCount.insert({wf.groupId, d}, stack + 1);

    FlowChartWorkflowNode node;
    node.workflowType = wf.workflowType;
    node.pos          = QPointF(40 + column * colW + stack * (nodeW + 12),
                                40 + groupLabelH + d * rowH);
    layout.workflowNodes.append(node);
  }

  // グループ枠はここでは生成しない
  // （シーン再構築時に、所属工程ノードの連結成分から常に自動導出される）

  return layout;
}
