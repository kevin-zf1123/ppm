#include "mainflowchartscene.h"

#include <QAbstractButton>
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QtMath>
#include <QSet>
#include <QMap>
#include <QPair>

namespace {
const qreal RoleIconDiameter  = 36;
const qreal RoleIconSpacing   = 6;
const qreal RolePaletteMargin = 8;

QColor textColorFor(const QColor& bg) {
  return (bg.lightness() > 128) ? QColor(25, 25, 25) : QColor(240, 240, 240);
}
}  // namespace

//--------------------------------
// MainWorkflowNodeItem
//--------------------------------

namespace {
// ハイライトの縁取りを輪郭線の外側に描くために、boundingRect()をウィジェット本体より
// 広めに確保しておく余白
const qreal HighlightMargin = 6;
}  // namespace

MainWorkflowNodeItem::MainWorkflowNodeItem(WorkflowType wfType,
                                           const QColor& bgColor,
                                           QWidget* contentWidget)
    : m_wfType(wfType), m_bgColor(bgColor) {
  setWidget(contentWidget);
  setZValue(10);
}

QRectF MainWorkflowNodeItem::widgetRect() const {
  return QRectF(QPointF(0, 0), widget() ? QSizeF(widget()->size()) : QSizeF());
}

QRectF MainWorkflowNodeItem::boundingRect() const {
  return widgetRect().adjusted(-HighlightMargin, -HighlightMargin,
                               HighlightMargin, HighlightMargin);
}

QPointF MainWorkflowNodeItem::inputPortScenePos() const {
  return mapToScene(QPointF(widgetRect().width() / 2, 0));
}

QPointF MainWorkflowNodeItem::outputPortScenePos() const {
  return mapToScene(QPointF(widgetRect().width() / 2, widgetRect().height()));
}

void MainWorkflowNodeItem::paint(QPainter* painter,
                                 const QStyleOptionGraphicsItem* option,
                                 QWidget* widget) {
  // QGraphicsProxyWidget上ではQSSのborder-radiusが確実に反映されないため、角丸背景を自前で描く。
  // 埋め込みwidget自体は透明背景にしてあるので、この上にチェックボックス等がそのまま重なる。
  // 輪郭線・角丸半径はProject
  // Settings側（flowchartscene.cppのWorkflowNodeItem::paint()）と揃える
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setPen(QPen(m_bgColor.darker(135), 1));
  painter->setBrush(m_bgColor);
  painter->drawRoundedRect(widgetRect(), 9, 9);
  if (m_highlighted) {
    // カット検索結果のハイライト：白い縁取りを、通常の輪郭線の外側に重ねて描く
    painter->setPen(QPen(Qt::white, 3));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(widgetRect().adjusted(-3, -3, 3, 3), 9, 9);
  }
  QGraphicsProxyWidget::paint(painter, option, widget);
}

//--------------------------------
// MainGroupNodeItem
//--------------------------------

MainGroupNodeItem::MainGroupNodeItem(FlowGroupId fgId, QWidget* contentWidget)
    : m_fgId(fgId) {
  setWidget(contentWidget);
  setZValue(-10);
}

void MainGroupNodeItem::paint(QPainter* painter,
                              const QStyleOptionGraphicsItem* option,
                              QWidget* widget) {
  // 背景色・輪郭線ともProject
  // Settings側（flowchartscene.cppのGroupNodeItem::paint()）と揃える
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setPen(QPen(QColor(120, 160, 150, 120), 1));
  painter->setBrush(QColor(150, 195, 185, 90));
  painter->drawRoundedRect(boundingRect(), 14, 14);
  QGraphicsProxyWidget::paint(painter, option, widget);
}

//--------------------------------
// MainLinkItem
//--------------------------------

MainLinkItem::MainLinkItem() {
  setZValue(0);
  setPen(QPen(QColor(45, 45, 45), 2));
}

void MainLinkItem::setEndpoints(const QPointF& start, const QPointF& end) {
  qreal dy = qMax<qreal>(30, qAbs(end.y() - start.y()) * 0.4);
  QPainterPath path(start);
  path.cubicTo(start + QPointF(0, dy), end - QPointF(0, dy), end);
  setPath(path);
}

//--------------------------------
// MainFlowChartView
//--------------------------------

MainFlowChartView::MainFlowChartView(QWidget* parent) : QGraphicsView(parent) {
  setRenderHint(QPainter::Antialiasing);
  setRenderHint(QPainter::TextAntialiasing);
  setDragMode(QGraphicsView::NoDrag);
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  setResizeAnchor(QGraphicsView::AnchorViewCenter);
}

void MainFlowChartView::wheelEvent(QWheelEvent* event) {
  qreal factor   = qPow(1.0015, event->angleDelta().y());
  qreal newScale = transform().m11() * factor;
  if (newScale < 0.1 || newScale > 4.0) {
    event->accept();
    return;
  }
  scale(factor, factor);
  event->accept();
}

void MainFlowChartView::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::MiddleButton) {
    m_panning    = true;
    m_panLastPos = event->pos();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }
  QGraphicsView::mousePressEvent(event);
}

void MainFlowChartView::mouseMoveEvent(QMouseEvent* event) {
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

void MainFlowChartView::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::MiddleButton && m_panning) {
    m_panning = false;
    unsetCursor();
    event->accept();
    return;
  }
  QGraphicsView::mouseReleaseEvent(event);
}

void MainFlowChartView::attachRolePalette(MainRolePaletteWidget* rolePalette) {
  m_rolePalette = rolePalette;
  m_rolePalette->refresh();
}

void MainFlowChartView::focusOnRect(const QRectF& sceneRect) {
  if (sceneRect.isEmpty() || viewport()->size().isEmpty()) return;

  qreal scaleX = viewport()->width() / sceneRect.width();
  qreal scaleY = viewport()->height() / sceneRect.height();
  // sceneRect全体が収まる倍率にしつつ、100%（等倍）より拡大はしない
  qreal targetScale = qMin(qMin(scaleX, scaleY), 1.0);

  QTransform transform;
  transform.scale(targetScale, targetScale);
  setTransform(transform);
  centerOn(sceneRect.center());
}

void MainFlowChartView::resizeEvent(QResizeEvent* event) {
  QGraphicsView::resizeEvent(event);
  if (m_rolePalette) m_rolePalette->refresh();
}

//--------------------------------
// MainRolePaletteWidget
//--------------------------------

MainRolePaletteWidget::MainRolePaletteWidget(QWidget* parent)
    : QWidget(parent) {
  setToolTip(
      tr("Click a role to select/deselect all workflows using its color."));
}

QSize MainRolePaletteWidget::sizeHint() const {
  int count = WorkflowManager::instance()->roles().size();
  qreal h   = RolePaletteMargin * 2 + count * RoleIconDiameter +
            qMax(0, count - 1) * RoleIconSpacing;
  qreal w = RolePaletteMargin * 2 + RoleIconDiameter;
  return QSize((int)w, (int)qMax(h, (qreal)(RolePaletteMargin * 2)));
}

QRectF MainRolePaletteWidget::roleIconRect(int index) const {
  qreal y = RolePaletteMargin + index * (RoleIconDiameter + RoleIconSpacing);
  return QRectF(RolePaletteMargin, y, RoleIconDiameter, RoleIconDiameter);
}

int MainRolePaletteWidget::roleIndexAt(const QPoint& pos) const {
  const QList<Role>& roles = WorkflowManager::instance()->roles();
  for (int i = 0; i < roles.size(); i++) {
    if (roleIconRect(i).contains(pos)) return i;
  }
  return -1;
}

QList<QAbstractButton*> MainRolePaletteWidget::buttonsForRole(int index) const {
  const QList<Role>& roles = WorkflowManager::instance()->roles();
  if (index < 0 || index >= roles.size()) return {};
  // m_roleButtonCacheはrebuildFlowchartPanel()で一度だけ構築されたもの。
  // ここでWorkflowManager::workflow(type)を呼ぶと、クリック・再描画のたびにWorkflow構造体の
  // ディープコピーがボタン数×役職数だけ発生し、体感できるほど反応が遅くなるため使わない
  return m_roleButtonCache.value(roles.at(index).color.rgba());
}

void MainRolePaletteWidget::refresh() {
  updateGeometry();
  QSize sz = sizeHint();
  if (parentWidget()) {
    const int margin = 12;  // MainFlowChartViewの右下マージン
    setGeometry(parentWidget()->width() - sz.width() - margin,
                parentWidget()->height() - sz.height() - margin, sz.width(),
                sz.height());
    raise();
  } else {
    resize(sz);
  }
  update();
}

void MainRolePaletteWidget::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const QList<Role>& roles = WorkflowManager::instance()->roles();
  for (int i = 0; i < roles.size(); i++) {
    const Role& role = roles.at(i);
    QRectF rect      = roleIconRect(i);
    painter.setPen(QPen(role.color.darker(140), 1));
    painter.setBrush(role.color);
    painter.drawEllipse(rect);

    // このロールの色を持つ工程の集計チェック状態を、接尾辞の代わりにアイコン上へ表示する
    QList<QAbstractButton*> buttons = buttonsForRole(i);
    int checkedCount                = 0;
    for (auto button : buttons)
      if (button->isChecked()) checkedCount++;

    QString glyph;
    if (!buttons.isEmpty()) {
      if (checkedCount == buttons.size())
        glyph = QString::fromUtf8("\xE2\x9C\x93");  // レ点(チェックマーク)
      else if (checkedCount == 0)
        glyph = "";
      else
        glyph = "-";
    }
    if (!glyph.isEmpty()) {
      QFont font;
      font.setBold(true);
      font.setPointSizeF(18);
      painter.setFont(font);
      painter.setPen(textColorFor(role.color));
      painter.drawText(rect, Qt::AlignCenter, glyph);
    }
  }
}

void MainRolePaletteWidget::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  int index = roleIndexAt(event->pos());
  if (index < 0) return;

  QList<QAbstractButton*> buttons = buttonsForRole(index);
  if (buttons.isEmpty()) return;

  // 全て選択中なら解除、それ以外（一部選択中／未選択）なら全て選択する
  bool allChecked = true;
  for (auto button : buttons) {
    if (!button->isChecked()) {
      allChecked = false;
      break;
    }
  }
  bool newState = !allChecked;
  for (auto button : buttons) button->setChecked(newState);

  update();
  if (m_onSelectionChanged) m_onSelectionChanged();
}

//--------------------------------
// 共通アルゴリズム
//--------------------------------

QList<QList<WorkflowType>> groupWorkflowsByConnectedComponent(
    FlowGroupId fgId) {
  QList<QList<WorkflowType>> result;

  QList<WorkflowType> members;
  for (int i = 0; i < WorkflowManager::instance()->workflowCount(); i++) {
    Workflow wf = WorkflowManager::instance()->getWorkflow(i);
    if (wf.groupId == fgId) members.append(wf.workflowType);
  }
  if (members.isEmpty()) return result;

  QMap<WorkflowType, QList<WorkflowType>> adjacency;
  for (auto memberType : members) {
    Workflow wf = WorkflowManager::instance()->workflow(memberType);
    for (auto& protocol : wf.protocols) {
      for (auto& action : protocol.actions) {
        for (auto next : action.allNextFlows()) {
          if (!members.contains(next)) continue;
          adjacency[memberType].append(next);
          adjacency[next].append(memberType);
        }
      }
    }
  }

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
    result.append(component);
  }
  return result;
}

FlowChartLayout buildDefaultMainFlowChartLayout() {
  FlowChartLayout layout;

  const qreal nodeW       = 200;
  const qreal rowH        = 70;
  const qreal colW        = 320;
  const qreal groupLabelH = 34;

  QMap<WorkflowType, int> depth;
  for (int i = 0; i < WorkflowManager::instance()->workflowCount(); i++)
    depth.insert(WorkflowManager::instance()->getWorkflow(i).workflowType, 0);
  int n = WorkflowManager::instance()->workflowCount();
  for (int iter = 0; iter < n; iter++) {
    bool changed = false;
    for (int i = 0; i < WorkflowManager::instance()->workflowCount(); i++) {
      Workflow wf = WorkflowManager::instance()->getWorkflow(i);
      int d       = depth.value(wf.workflowType, 0);
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

  QList<FlowGroupId> groupOrder = WorkflowManager::instance()->flowGroupIds();
  QMap<FlowGroupId, int> columnIndex;
  for (int i = 0; i < groupOrder.size(); i++)
    columnIndex.insert(groupOrder.at(i), i);

  QMap<QPair<FlowGroupId, int>, int> stackCount;

  for (int i = 0; i < WorkflowManager::instance()->workflowCount(); i++) {
    Workflow wf = WorkflowManager::instance()->getWorkflow(i);
    int column  = columnIndex.value(wf.groupId, groupOrder.size());
    int d       = depth.value(wf.workflowType, 0);
    int stack   = stackCount.value({wf.groupId, d}, 0);
    stackCount.insert({wf.groupId, d}, stack + 1);

    FlowChartWorkflowNode node;
    node.workflowType = wf.workflowType;
    node.pos          = QPointF(40 + column * colW + stack * (nodeW + 12),
                                40 + groupLabelH + d * rowH);
    layout.workflowNodes.append(node);
  }

  return layout;
}
