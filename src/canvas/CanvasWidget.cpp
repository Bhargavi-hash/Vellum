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
#include <algorithm>

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
    for (int i = (int)strokes.size() - 1; i >= 0; --i) {
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
        s.pts.push_back(StrokePoint{dp.worldPos, dp.pressure, (int)dp.tMs});
    }

    doc_->undoStack()->push(new AddStrokeCommand(doc_, std::move(s)));
    if (smartShapesEnabled_) {
        const ShapeMatch m = ShapeRecognizer::recognize(doc_->strokes().back());
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
    const QPointF world = viewToWorld(e->position());

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
        appendStrokePoint(world, 1.0f);
        e->accept();
        return;
    }

    if (isDraggingText_ && activeTextId_ >= 0 && doc_) {
        const QPointF delta = world - dragStartWorld_;
        doc_->setTextBoxRectById(activeTextId_, dragStartRect_.translated(delta));
        update();
        e->accept();
        return;
    }

    if (tool_ == Tool::Eraser && (e->buttons() & Qt::LeftButton)) {
        eraseAt(world, 10.0 / zoom_);
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
        zoom_ = std::clamp(zoom_ * factor, 0.1, 12.0);
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
    panViewPx_ += QPointF(delta.x(), delta.y());
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
    QWidget::tabletEvent(e);
}

void CanvasWidget::drawPages(QPainter& p) const {
    constexpr double pageW = 595.0;
    constexpr double pageH = 842.0;
    constexpr double gap = 48.0;

    const QRectF worldView = viewToWorld(QRectF(QPointF(0, 0), QSizeF(size())));
    const double stride = pageH + gap;
    const int firstIdx = std::max(0, (int)std::floor(worldView.top() / stride));
    const int lastIdx = (int)std::ceil(worldView.bottom() / stride);

    p.fillRect(rect(), QColor(230, 230, 230));

    for (int i = firstIdx; i <= lastIdx; ++i) {
        const QRectF pageWorld(0.0, i * stride, pageW, pageH);
        const QRectF pageView(worldToView(pageWorld.topLeft()), worldToView(pageWorld.bottomRight()));

        p.fillRect(pageView.translated(3, 3), QColor(0, 0, 0, 20)); // Shadow
        p.fillRect(pageView, QColor(255, 255, 255));
        p.setPen(QPen(QColor(200, 200, 200), 1));
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

        if (s.isShape && !s.shapeType.isEmpty()) {
            float avg = 0.0f;
            for (const auto& pt : s.pts) avg += pt.pressure;
            avg /= std::max((qsizetype)1, s.pts.size());
            pen.setWidthF(std::max(0.5, s.baseWidthPoints * avg * zoom_));
            p.setPen(pen);

            if (s.shapeType == "line") {
                QDataStream ds(s.shapeParams);
                QPointF a, b; ds >> a >> b;
                p.drawLine(worldToView(a), worldToView(b));
                return;
            } else if (s.shapeType == "circle") {
                QDataStream ds(s.shapeParams);
                QPointF c; double r; ds >> c >> r;
                QRectF rv(worldToView(QPointF(c.x()-r, c.y()-r)), worldToView(QPointF(c.x()+r, c.y()+r)));
                p.drawEllipse(rv.normalized());
                return;
            }
        }

        for (int i = 1; i < s.pts.size(); ++i) {
            const auto& a = s.pts[i - 1];
            const auto& b = s.pts[i];
            float pr = (a.pressure + b.pressure) * 0.5f;
            pen.setWidthF(std::max(0.5, s.baseWidthPoints * pr * zoom_));
            p.setPen(pen);
            p.drawLine(worldToView(a.worldPos), worldToView(b.worldPos));
        }
    };

    if (doc_) {
        for (const auto& s : doc_->strokes()) drawStroke(s);
    }

    if (isDrawing_ && draft_.size() >= 2) {
        Stroke s;
        s.color = draftColor_;
        s.baseWidthPoints = draftBaseWidthPoints_;
        for (const auto& dp : draft_) s.pts.push_back(StrokePoint{dp.worldPos, dp.pressure, (int)dp.tMs});
        drawStroke(s);
    }
}

void CanvasWidget::paintEvent(QPaintEvent* e) {
    Q_UNUSED(e);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (viewMode_ == ViewMode::A4Notebook) {
        drawPages(p);
    } else {
        p.fillRect(rect(), palette().color(QPalette::Base));
    }

    drawStrokes(p);
    drawTextBoxes(p);
}

qint64 CanvasWidget::hitTestTextBox(const QPointF& worldPos) const {
    if (!doc_) return -1;
    for (int i = (int)doc_->textBoxes().size() - 1; i >= 0; --i) {
        if (doc_->textBoxes()[i].rectWorld.contains(worldPos)) return doc_->textBoxes()[i].id;
    }
    return -1;
}

void CanvasWidget::startEditingTextBox(qint64 id) {
    if (!doc_ || id < 0) return;
    activeTextId_ = id;
    int idx = doc_->textBoxIndexById(id);
    if (idx < 0) return;

    if (!editor_) {
        editor_ = new QPlainTextEdit(this);
        editor_->setFrameStyle(QFrame::NoFrame);
        editorCommitTimer_ = new QTimer(this);
        editorCommitTimer_->setSingleShot(true);
        connect(editor_, &QPlainTextEdit::textChanged, [this]() { editorCommitTimer_->start(350); });
        connect(editorCommitTimer_, &QTimer::timeout, [this]() {
            if (activeTextId_ < 0) return;
            int i = doc_->textBoxIndexById(activeTextId_);
            QString after = editor_->toPlainText();
            doc_->undoStack()->push(new SetTextBoxMarkdownCommand(doc_, activeTextId_, doc_->textBoxes()[i].markdown, after));
        });
    }
    const auto& tb = doc_->textBoxes()[idx];
    QRectF vr(worldToView(tb.rectWorld.topLeft()), worldToView(tb.rectWorld.bottomRight()));
    editor_->setGeometry(vr.normalized().toRect());
    editor_->setPlainText(tb.markdown);
    editor_->show();
    editor_->setFocus();
}

void CanvasWidget::drawTextBoxes(QPainter& p) const {
    if (!doc_) return;
    for (const auto& tb : doc_->textBoxes()) {
        QRectF vr(worldToView(tb.rectWorld.topLeft()), worldToView(tb.rectWorld.bottomRight()));
        p.setPen(QColor(180, 180, 180));
        p.setBrush(QColor(255, 255, 255, 200));
        p.drawRoundedRect(vr.normalized(), 4, 4);
        QTextDocument d;
        d.setMarkdown(tb.markdown);
        p.save();
        p.translate(vr.topLeft() + QPointF(5, 5));
        d.drawContents(&p);
        p.restore();
    }
}

QRectF CanvasWidget::currentViewportWorld() const {
    return viewToWorld(QRectF(QPointF(0, 0), QSizeF(size())));
}