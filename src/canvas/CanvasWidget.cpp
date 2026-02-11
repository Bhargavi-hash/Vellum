#include "CanvasWidget.h"

#include <QtMath>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QTabletEvent>
#include <QWheelEvent>

CanvasWidget::CanvasWidget(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setAutoFillBackground(true);
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setFocusPolicy(Qt::StrongFocus);
  timer_.start();
}

void CanvasWidget::setTool(Tool tool) {
  tool_ = tool;
  isDrawing_ = false;
  current_ = Stroke{};
  update();
}

void CanvasWidget::setViewMode(ViewMode mode) {
  viewMode_ = mode;
  update();
}

void CanvasWidget::setPenColor(const QColor& c) {
  penColor_ = c;
  update();
}

void CanvasWidget::setPenWidthPoints(double w) {
  penWidthPoints_ = w;
  update();
}

void CanvasWidget::setSmartShapesEnabled(bool enabled) {
  smartShapesEnabled_ = enabled;
  update();
}

QPointF CanvasWidget::viewToWorld(const QPointF& viewPos) const {
  return (viewPos - panViewPx_) / zoom_;
}

QPointF CanvasWidget::worldToView(const QPointF& worldPos) const {
  return (worldPos * zoom_) + panViewPx_;
}

QRectF CanvasWidget::viewToWorld(const QRectF& viewRect) const {
  const QPointF tl = viewToWorld(viewRect.topLeft());
  const QPointF br = viewToWorld(viewRect.bottomRight());
  return QRectF(tl, br).normalized();
}

void CanvasWidget::beginStroke(const QPointF& worldPos, float pressure) {
  isDrawing_ = true;
  current_ = Stroke{};
  current_.color = penColor_;
  current_.baseWidthPoints = penWidthPoints_;
  current_.pts.reserve(512);
  current_.pts.push_back(StrokePoint{worldPos, pressure, timer_.elapsed()});
  update();
}

void CanvasWidget::appendStrokePoint(const QPointF& worldPos, float pressure) {
  if (!isDrawing_) return;
  if (!current_.pts.isEmpty()) {
    const QPointF prev = current_.pts.back().worldPos;
    const double minDistWorld = 0.3;  // ~0.3pt sampling threshold
    if (QLineF(prev, worldPos).length() < minDistWorld) return;
  }
  current_.pts.push_back(StrokePoint{worldPos, pressure, timer_.elapsed()});
  update();
}

void CanvasWidget::endStroke() {
  if (!isDrawing_) return;
  isDrawing_ = false;
  if (current_.pts.size() >= 2) strokes_.push_back(current_);
  current_ = Stroke{};
  update();
}

static double distPointToSegment(const QPointF& p, const QPointF& a, const QPointF& b) {
  const QPointF ab = b - a;
  const QPointF ap = p - a;
  const double ab2 = QPointF::dotProduct(ab, ab);
  if (ab2 <= 1e-9) return QLineF(p, a).length();
  double t = QPointF::dotProduct(ap, ab) / ab2;
  t = std::clamp(t, 0.0, 1.0);
  const QPointF proj = a + ab * t;
  return QLineF(p, proj).length();
}

void CanvasWidget::eraseAt(const QPointF& worldPos, double radiusWorld) {
  // MVP eraser: delete whole strokes that come within radius.
  for (int i = strokes_.size() - 1; i >= 0; --i) {
    const auto& s = strokes_[i];
    bool hit = false;
    for (int j = 1; j < s.pts.size(); ++j) {
      const QPointF a = s.pts[j - 1].worldPos;
      const QPointF b = s.pts[j].worldPos;
      if (distPointToSegment(worldPos, a, b) <= radiusWorld) {
        hit = true;
        break;
      }
    }
    if (hit) {
      strokes_.removeAt(i);
      update();
      return;
    }
  }
}

void CanvasWidget::mousePressEvent(QMouseEvent* e) {
  if (e->button() == Qt::MiddleButton || e->button() == Qt::RightButton) {
    isPanning_ = true;
    lastPanViewPos_ = e->position();
    e->accept();
    return;
  }

  if (e->button() != Qt::LeftButton) return;
  const QPointF world = viewToWorld(e->position());

  if (tool_ == Tool::Pen) {
    beginStroke(world, 1.0f);
    e->accept();
    return;
  }
  if (tool_ == Tool::Eraser) {
    eraseAt(world, 10.0 / zoom_);
    e->accept();
    return;
  }
  QWidget::mousePressEvent(e);
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* e) {
  if (isPanning_) {
    const QPointF now = e->position();
    const QPointF delta = now - lastPanViewPos_;
    panViewPx_ += delta;
    lastPanViewPos_ = now;
    update();
    e->accept();
    return;
  }

  if (tool_ == Tool::Pen && isDrawing_) {
    appendStrokePoint(viewToWorld(e->position()), 1.0f);
    e->accept();
    return;
  }
  if (tool_ == Tool::Eraser && (e->buttons() & Qt::LeftButton)) {
    eraseAt(viewToWorld(e->position()), 10.0 / zoom_);
    e->accept();
    return;
  }
  QWidget::mouseMoveEvent(e);
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* e) {
  if (isPanning_ && (e->button() == Qt::MiddleButton || e->button() == Qt::RightButton)) {
    isPanning_ = false;
    e->accept();
    return;
  }

  if (tool_ == Tool::Pen && e->button() == Qt::LeftButton) {
    endStroke();
    e->accept();
    return;
  }
  QWidget::mouseReleaseEvent(e);
}

void CanvasWidget::wheelEvent(QWheelEvent* e) {
  // Ctrl+wheel zoom; otherwise pan/scroll.
  if (e->modifiers().testFlag(Qt::ControlModifier)) {
    const QPointF anchorView = e->position();
    const QPointF anchorWorld = viewToWorld(anchorView);

    const double steps = e->angleDelta().y() / 120.0;
    const double factor = std::pow(1.15, steps);
    const double newZoom = std::clamp(zoom_ * factor, 0.1, 12.0);

    zoom_ = newZoom;
    // Keep anchorWorld under cursor.
    panViewPx_ = anchorView - (anchorWorld * zoom_);
    update();
    e->accept();
    return;
  }

  // Basic scroll/pan.
  const QPoint numPx = e->pixelDelta();
  const QPoint numDeg = e->angleDelta();
  QPointF delta;
  if (!numPx.isNull()) {
    delta = QPointF(numPx);
  } else if (!numDeg.isNull()) {
    delta = QPointF(numDeg.x() / 6.0, numDeg.y() / 6.0);  // heuristic
  }
  panViewPx_ += QPointF(-delta.x(), -delta.y());
  update();
  e->accept();
}

void CanvasWidget::tabletEvent(QTabletEvent* e) {
  const QPointF world = viewToWorld(e->position());
  const float pressure = std::clamp(static_cast<float>(e->pressure()), 0.0f, 1.0f);

  if (tool_ == Tool::Pen) {
    if (e->type() == QEvent::TabletPress) {
      beginStroke(world, pressure);
      e->accept();
      return;
    }
    if (e->type() == QEvent::TabletMove) {
      appendStrokePoint(world, pressure);
      e->accept();
      return;
    }
    if (e->type() == QEvent::TabletRelease) {
      endStroke();
      e->accept();
      return;
    }
  }

  if (tool_ == Tool::Eraser) {
    if (e->type() == QEvent::TabletPress || e->type() == QEvent::TabletMove) {
      eraseAt(world, 10.0 / zoom_);
      e->accept();
      return;
    }
  }

  QWidget::tabletEvent(e);
}

void CanvasWidget::drawPages(QPainter& p) const {
  // A4 @ 72dpi points: 595x842.
  constexpr double pageW = 595.0;
  constexpr double pageH = 842.0;
  constexpr double gap = 48.0;

  const QRectF worldView = viewToWorld(QRectF(QPointF(0, 0), QSizeF(size())));
  const double stride = pageH + gap;
  const int firstIdx = std::max(0, static_cast<int>(std::floor(worldView.top() / stride)) - 1);
  const int lastIdx = static_cast<int>(std::ceil(worldView.bottom() / stride)) + 1;

  // Background behind pages.
  p.fillRect(rect(), QColor(245, 245, 245));

  for (int i = firstIdx; i <= lastIdx; ++i) {
    const QRectF pageWorld(0.0, i * stride, pageW, pageH);
    const QRectF pageView(worldToView(pageWorld.topLeft()), worldToView(pageWorld.bottomRight()));

    // Drop shadow (very subtle).
    p.fillRect(pageView.translated(2, 2), QColor(0, 0, 0, 18));
    p.fillRect(pageView, QColor(255, 255, 255));
    p.setPen(QPen(QColor(210, 210, 210), 1));
    p.drawRect(pageView);
  }
}

void CanvasWidget::drawStrokes(QPainter& p) const {
  auto drawStroke = [&](const Stroke& s) {
    if (s.pts.size() < 2) return;
    QPen pen;
    pen.setColor(s.color);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);

    for (int i = 1; i < s.pts.size(); ++i) {
      const auto& a = s.pts[i - 1];
      const auto& b = s.pts[i];
      const float pr = (a.pressure + b.pressure) * 0.5f;
      const double wPx = std::max(0.5, s.baseWidthPoints * pr * zoom_);
      pen.setWidthF(wPx);
      p.setPen(pen);
      p.drawLine(worldToView(a.worldPos), worldToView(b.worldPos));
    }
  };

  for (const auto& s : strokes_) drawStroke(s);
  if (isDrawing_) drawStroke(current_);
}

void CanvasWidget::paintEvent(QPaintEvent* e) {
  Q_UNUSED(e);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  if (viewMode_ == ViewMode::A4Notebook) {
    drawPages(p);
  } else {
    const QColor bg = palette().color(QPalette::Base);
    p.fillRect(rect(), bg);
  }

  drawStrokes(p);
}

