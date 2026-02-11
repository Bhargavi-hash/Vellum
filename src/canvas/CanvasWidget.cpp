#include "CanvasWidget.h"

#include <QtMath>

#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QTabletEvent>
#include <QWheelEvent>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QTimer>

#include "model/Commands.h"
#include "model/Document.h"
#include "shapes/ShapeRecognizer.h"

CanvasWidget::CanvasWidget(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setAutoFillBackground(true);
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setFocusPolicy(Qt::StrongFocus);
  timer_.start();
}

void CanvasWidget::setDocument(Document* doc) {
  if (doc_ == doc) return;
  if (doc_) disconnect(doc_, nullptr, this, nullptr);
  doc_ = doc;
  if (doc_) {
    connect(doc_, &Document::changed, this, [this]() { update(); });
  }
  update();
}

void CanvasWidget::setTool(Tool tool) {
  tool_ = tool;
  isDrawing_ = false;
  draft_.clear();
  update();
}

void CanvasWidget::setViewMode(ViewMode mode) {
  viewMode_ = mode;
  if (doc_) {
    doc_->setViewMode(mode == ViewMode::A4Notebook ? Document::ViewMode::A4Notebook
                                                   : Document::ViewMode::Infinite);
  }
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
  draft_.clear();
  draft_.reserve(512);
  draftColor_ = penColor_;
  draftBaseWidthPoints_ = penWidthPoints_;
  draft_.push_back(DraftPoint{worldPos, pressure, timer_.elapsed()});
  update();
}

void CanvasWidget::appendStrokePoint(const QPointF& worldPos, float pressure) {
  if (!isDrawing_) return;
  if (!draft_.isEmpty()) {
    const QPointF prev = draft_.back().worldPos;
    const double minDistWorld = 0.3;
    if (QLineF(prev, worldPos).length() < minDistWorld) return;
  }
  draft_.push_back(DraftPoint{worldPos, pressure, timer_.elapsed()});
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
  if (!doc_) return;

  const auto& strokes = doc_->strokes();
  for (int i = strokes.size() - 1; i >= 0; --i) {
    const auto& s = strokes[i];
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
      doc_->undoStack()->push(new RemoveStrokeCommand(doc_, i));
      return;
    }
  }
}

void CanvasWidget::endStroke() {
  if (!isDrawing_) return;
  isDrawing_ = false;

  if (!doc_ || draft_.size() < 2) {
    draft_.clear();
    update();
    return;
  }

  Stroke s;
  s.id = doc_->nextStrokeId();
  s.color = draftColor_;
  s.baseWidthPoints = draftBaseWidthPoints_;
  s.pts.reserve(draft_.size());
  for (const auto& dp : draft_) {
    s.pts.push_back(StrokePoint{dp.worldPos, dp.pressure, dp.tMs});
  }

  doc_->undoStack()->push(new AddStrokeCommand(doc_, std::move(s)));
  if (smartShapesEnabled_) {
    const ShapeMatch m = ShapeRecognizer::recognize(s);
    if (m.matched && m.score >= 0.6) {
      doc_->undoStack()->push(new SetStrokeShapeCommand(doc_, s.id, true, m.type, m.params));
    }
  }
  draft_.clear();
  update();
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
  if (tool_ == Tool::Text) {
    if (!doc_) return;
    TextBox tb;
    tb.id = doc_->nextTextBoxId();
    tb.rectWorld = QRectF(world, QSizeF(260, 120));
    tb.markdown = "";
    doc_->undoStack()->push(new AddTextBoxCommand(doc_, tb));
    startEditingTextBox(tb.id);
    e->accept();
    return;
  }
  if (tool_ == Tool::Select) {
    const qint64 hit = hitTestTextBox(world);
    if (hit >= 0) {
      activeTextId_ = hit;
      isDraggingText_ = true;
      dragStartWorld_ = world;
      const int idx = doc_ ? doc_->textBoxIndexById(hit) : -1;
      if (idx >= 0) dragStartRect_ = doc_->textBoxes()[idx].rectWorld;
      e->accept();
      return;
    }
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
  if (isDraggingText_ && activeTextId_ >= 0 && doc_) {
    const QPointF delta = world - dragStartWorld_;
    doc_->setTextBoxRectById(activeTextId_, dragStartRect_.translated(delta));
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

  if (isDraggingText_ && e->button() == Qt::LeftButton && doc_ && activeTextId_ >= 0) {
    isDraggingText_ = false;
    const int idx = doc_->textBoxIndexById(activeTextId_);
    if (idx >= 0) {
      const QRectF after = doc_->textBoxes()[idx].rectWorld;
      if (after != dragStartRect_) {
        doc_->undoStack()->push(new SetTextBoxRectCommand(doc_, activeTextId_, dragStartRect_, after));
      }
    }
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
  if (e->modifiers().testFlag(Qt::ControlModifier)) {
    const QPointF anchorView = e->position();
    const QPointF anchorWorld = viewToWorld(anchorView);

    const double steps = e->angleDelta().y() / 120.0;
    const double factor = std::pow(1.15, steps);
    const double newZoom = std::clamp(zoom_ * factor, 0.1, 12.0);

    zoom_ = newZoom;
    panViewPx_ = anchorView - (anchorWorld * zoom_);
    update();
    e->accept();
    return;
  }

  const QPoint numPx = e->pixelDelta();
  const QPoint numDeg = e->angleDelta();
  QPointF delta;
  if (!numPx.isNull()) {
    delta = QPointF(numPx);
  } else if (!numDeg.isNull()) {
    delta = QPointF(numDeg.x() / 6.0, numDeg.y() / 6.0);
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
  constexpr double pageW = 595.0;
  constexpr double pageH = 842.0;
  constexpr double gap = 48.0;

  const QRectF worldView = viewToWorld(QRectF(QPointF(0, 0), QSizeF(size())));
  const double stride = pageH + gap;
  const int firstIdx = std::max(0, static_cast<int>(std::floor(worldView.top() / stride)) - 1);
  const int lastIdx = static_cast<int>(std::ceil(worldView.bottom() / stride)) + 1;

  p.fillRect(rect(), QColor(245, 245, 245));

  for (int i = firstIdx; i <= lastIdx; ++i) {
    const QRectF pageWorld(0.0, i * stride, pageW, pageH);
    const QRectF pageView(worldToView(pageWorld.topLeft()), worldToView(pageWorld.bottomRight()));

    p.fillRect(pageView.translated(2, 2), QColor(0, 0, 0, 18));
    p.fillRect(pageView, QColor(255, 255, 255));
    p.setPen(QPen(QColor(210, 210, 210), 1));
    p.drawRect(pageView);
  }
}

void CanvasWidget::drawStrokes(QPainter& p) const {
  auto drawStroke = [&](const Stroke& s) {
    if (s.pts.size() < 2) return;

    // If snapped, render the perfect path.
    if (s.isShape && !s.shapeType.isEmpty()) {
      QPen pen;
      pen.setColor(s.color);
      pen.setCapStyle(Qt::RoundCap);
      pen.setJoinStyle(Qt::RoundJoin);
      // Use average pressure for width.
      float avg = 0.0f;
      for (const auto& p : s.pts) avg += p.pressure;
      avg /= std::max(1, s.pts.size());
      pen.setWidthF(std::max(0.5, s.baseWidthPoints * avg * zoom_));
      p.setPen(pen);

      ShapeMatch match;
      // Recreate the path from stored params for deterministic redraw.
      if (s.shapeType == "line") {
        QDataStream ds(s.shapeParams);
        ds.setVersion(QDataStream::Qt_6_0);
        QPointF a, b;
        ds >> a >> b;
        QPainterPath path;
        path.moveTo(worldToView(a));
        path.lineTo(worldToView(b));
        p.drawPath(path);
        return;
      }
      if (s.shapeType == "circle") {
        QDataStream ds(s.shapeParams);
        ds.setVersion(QDataStream::Qt_6_0);
        QPointF c;
        double r = 0;
        ds >> c >> r;
        const QRectF rv(worldToView(QPointF(c.x() - r, c.y() - r)),
                        worldToView(QPointF(c.x() + r, c.y() + r)));
        p.drawEllipse(rv.normalized());
        return;
      }
      if (s.shapeType == "rect") {
        QDataStream ds(s.shapeParams);
        ds.setVersion(QDataStream::Qt_6_0);
        QRectF r;
        ds >> r;
        const QRectF rv(worldToView(r.topLeft()), worldToView(r.bottomRight()));
        p.drawRect(rv.normalized());
        return;
      }
      // Fall through if unknown.
    }

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

};

  if (doc_) {
    for (const auto& s : doc_->strokes()) drawStroke(s);
  }

  // Draw current draft stroke on top.
  if (isDrawing_ && draft_.size() >= 2) {
    Stroke s;
    s.color = draftColor_;
    s.baseWidthPoints = draftBaseWidthPoints_;
    s.pts.reserve(draft_.size());
    for (const auto& dp : draft_) s.pts.push_back(StrokePoint{dp.worldPos, dp.pressure, dp.tMs});
    drawStroke(s);
  }
}

void CanvasWidget::paintEvent(QPaintEvent* e) {
  Q_UNUSED(e);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  const bool a4 = (viewMode_ == ViewMode::A4Notebook) ||
                  (doc_ && doc_->viewMode() == Document::ViewMode::A4Notebook);

  if (a4) {
    drawPages(p);
  } else {
    const QColor bg = palette().color(QPalette::Base);
    p.fillRect(rect(), bg);
  }

  drawStrokes(p);
  drawTextBoxes(p);

  if (editor_ && editor_->isVisible() && doc_ && activeTextId_ >= 0) {
    const int idx = doc_->textBoxIndexById(activeTextId_);
    if (idx >= 0) {
      const QRectF r = doc_->textBoxes()[idx].rectWorld;
      const QRectF vr(worldToView(r.topLeft()), worldToView(r.bottomRight()));
      editor_->setGeometry(vr.normalized().toRect().adjusted(2, 2, -2, -2));
    }
  }
}


qint64 CanvasWidget::hitTestTextBox(const QPointF& worldPos) const {
  if (!doc_) return -1;
  for (int i = doc_->textBoxes().size() - 1; i >= 0; --i) {
    const auto& t = doc_->textBoxes()[i];
    if (t.rectWorld.contains(worldPos)) return t.id;
  }
  return -1;
}

void CanvasWidget::startEditingTextBox(qint64 id) {
  if (!doc_ || id < 0) return;
  const int idx = doc_->textBoxIndexById(id);
  if (idx < 0) return;
  activeTextId_ = id;
  const auto& tb = doc_->textBoxes()[idx];

  if (!editor_) {
    editor_ = new QPlainTextEdit(this);
    editor_->setFrameStyle(QFrame::NoFrame);
    editor_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editor_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editor_->setTabChangesFocus(true);
    editor_->hide();

    editorCommitTimer_ = new QTimer(this);
    editorCommitTimer_->setSingleShot(true);

    connect(editor_, &QPlainTextEdit::textChanged, this, [this]() {
      // Debounce commits to keep typing responsive and keep undo stack sane.
      editorCommitTimer_->start(350);
    });

    connect(editorCommitTimer_, &QTimer::timeout, this, [this]() {
      if (!doc_ || activeTextId_ < 0) return;
      const int i = doc_->textBoxIndexById(activeTextId_);
      if (i < 0) return;
      const QString before = doc_->textBoxes()[i].markdown;
      const QString after = editor_->toPlainText();
      if (before == after) return;
      doc_->undoStack()->push(new SetTextBoxMarkdownCommand(doc_, activeTextId_, before, after));
    });
  }

  // Position editor over the text box.
  const QRectF vr(worldToView(tb.rectWorld.topLeft()), worldToView(tb.rectWorld.bottomRight()));
  editor_->setGeometry(vr.normalized().toRect().adjusted(2, 2, -2, -2));
  editor_->setPlainText(tb.markdown);
  editor_->show();
  editor_->setFocus(Qt::MouseFocusReason);
}

void CanvasWidget::drawTextBoxes(QPainter& p) const {
  if (!doc_) return;

  for (const auto& tb : doc_->textBoxes()) {
    const QRectF vr(worldToView(tb.rectWorld.topLeft()), worldToView(tb.rectWorld.bottomRight()));
    const QRectF box = vr.normalized();

    // Minimal box chrome.
    p.setPen(QPen(QColor(180,180,180), 1));
    p.setBrush(QColor(255,255,255,200));
    p.drawRoundedRect(box, 6, 6);

    QTextDocument doc;
    doc.setMarkdown(tb.markdown);
    doc.setTextWidth(std::max(1.0, box.width() - 10.0));

    p.save();
    p.translate(box.topLeft() + QPointF(5, 4));
    doc.drawContents(&p);
    p.restore();
  }
}


QRectF CanvasWidget::currentViewportWorld() const {
  return viewToWorld(QRectF(QPointF(0, 0), QSizeF(size())));
}
