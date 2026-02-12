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
#include <QKeyEvent>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <algorithm>

#include "model/Commands.h"
#include "model/Document.h"
#include "shapes/ShapeRecognizer.h"

// Constant for the resize handle hit area
const double kHandleSizeView = 12.0;

CanvasWidget::CanvasWidget(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setAutoFillBackground(true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setFocusPolicy(Qt::StrongFocus);
    timer_.start();
}

void CanvasWidget::setDocument(Document *doc)
{
    if (doc_ == doc) return;
    if (doc_) disconnect(doc_, nullptr, this, nullptr);
    doc_ = doc;
    if (doc_) {
        connect(doc_, &Document::changed, this, [this]() { update(); });
    }
    update();
}

void CanvasWidget::setTool(Tool tool)
{
    tool_ = tool;
    isDrawing_ = false;
    draft_.clear();
    if (editor_ && tool != Tool::Text) {
        editor_->hide();
    }
    update();
}

void CanvasWidget::setViewMode(ViewMode mode)
{
    viewMode_ = mode;
    if (doc_) {
        doc_->setViewMode(mode == ViewMode::A4Notebook ? Document::ViewMode::A4Notebook : Document::ViewMode::Infinite);
    }
    update();
}

void CanvasWidget::setPenColor(const QColor &c) { penColor_ = c; update(); }
void CanvasWidget::setPenWidthPoints(double w) { penWidthPoints_ = w; update(); }
void CanvasWidget::setSmartShapesEnabled(bool enabled) { smartShapesEnabled_ = enabled; update(); }

QPointF CanvasWidget::viewToWorld(const QPointF &viewPos) const { return (viewPos - panViewPx_) / zoom_; }
QPointF CanvasWidget::worldToView(const QPointF &worldPos) const { return (worldPos * zoom_) + panViewPx_; }

QRectF CanvasWidget::viewToWorld(const QRectF &viewRect) const {
    const QPointF tl = viewToWorld(viewRect.topLeft());
    const QPointF br = viewToWorld(viewRect.bottomRight());
    return QRectF(tl, br).normalized();
}

void CanvasWidget::beginStroke(const QPointF &worldPos, float pressure)
{
    isDrawing_ = true;
    draft_.clear();
    draft_.reserve(512);
    draftColor_ = penColor_;
    draftBaseWidthPoints_ = penWidthPoints_;
    draft_.push_back(DraftPoint{worldPos, pressure, timer_.elapsed()});
    update();
}

void CanvasWidget::appendStrokePoint(const QPointF &worldPos, float pressure)
{
    if (!isDrawing_) return;
    if (!draft_.isEmpty()) {
        const QPointF prev = draft_.back().worldPos;
        if (QLineF(prev, worldPos).length() < 0.3) return;
    }
    draft_.push_back(DraftPoint{worldPos, pressure, timer_.elapsed()});
    update();
}

static double distPointToSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const double ab2 = QPointF::dotProduct(ab, ab);
    if (ab2 <= 1e-9) return QLineF(p, a).length();
    double t = std::clamp(QPointF::dotProduct(p - a, ab) / ab2, 0.0, 1.0);
    return QLineF(p, a + ab * t).length();
}

void CanvasWidget::eraseAt(const QPointF &worldPos, double radiusWorld)
{
    if (!doc_) return;
    const auto &strokes = doc_->strokes();
    for (int i = (int)strokes.size() - 1; i >= 0; --i) {
        bool hit = false;
        for (int j = 1; j < strokes[i].pts.size(); ++j) {
            if (distPointToSegment(worldPos, strokes[i].pts[j - 1].worldPos, strokes[i].pts[j].worldPos) <= radiusWorld) {
                hit = true; break;
            }
        }
        if (hit) {
            doc_->undoStack()->push(new RemoveStrokeCommand(doc_, i));
            return;
        }
    }
}

void CanvasWidget::endStroke()
{
    if (!isDrawing_) return;
    isDrawing_ = false;
    if (!doc_ || draft_.size() < 2) { draft_.clear(); update(); return; }

    Stroke s;
    s.id = doc_->nextStrokeId();
    s.color = draftColor_;
    s.baseWidthPoints = draftBaseWidthPoints_;
    for (const auto &dp : draft_) s.pts.push_back(StrokePoint{dp.worldPos, dp.pressure, (int)dp.tMs});

    doc_->undoStack()->push(new AddStrokeCommand(doc_, std::move(s)));
    if (smartShapesEnabled_) {
        const ShapeMatch m = ShapeRecognizer::recognize(doc_->strokes().back());
        if (m.matched && m.score >= 0.7) {
            doc_->undoStack()->push(new SetStrokeShapeCommand(doc_, s.id, true, m.type, m.params));
        }
    }
    draft_.clear();
    update();
}

void CanvasWidget::mousePressEvent(QMouseEvent *e)
{
    const QPointF world = viewToWorld(e->position());

    if (e->button() == Qt::RightButton) {
        qint64 hit = hitTestTextBox(world);
        if (hit >= 0) {
            QMenu menu(this);
            QAction* copyAct = menu.addAction("Copy Text");
            QAction* delAct = menu.addAction("Delete Box");
            QAction* selected = menu.exec(e->globalPosition().toPoint());
            
            if (selected == copyAct) {
                int idx = doc_->textBoxIndexById(hit);
                QApplication::clipboard()->setText(doc_->textBoxes()[idx].markdown);
            } else if (selected == delAct) {
                doc_->undoStack()->push(new RemoveTextBoxCommand(doc_, doc_->textBoxIndexById(hit)));
                activeTextId_ = -1;
            }
            update(); return;
        }
        isPanning_ = true; lastPanViewPos_ = e->position(); e->accept(); return;
    }

    if (e->button() == Qt::MiddleButton) {
        isPanning_ = true; lastPanViewPos_ = e->position(); e->accept(); return;
    }

    if (e->button() != Qt::LeftButton) return;

    if (tool_ == Tool::Pen) { beginStroke(world, 1.0f); e->accept(); }
    else if (tool_ == Tool::Eraser) { eraseAt(world, 10.0 / zoom_); e->accept(); }
    else if (tool_ == Tool::Text) {
        // First, check if we are clicking an existing box
        const qint64 hit = hitTestTextBox(world);
        if (hit >= 0) {
            // If we hit an existing one, just edit it!
            startEditingTextBox(hit);
        } else {
            // Only create a new box if we clicked empty space
            if (!doc_) return;
            TextBox tb;
            tb.id = doc_->nextTextBoxId();
            // Default size, or you could make this dynamic
            tb.rectWorld = QRectF(world, QSizeF(200 / zoom_, 100 / zoom_));
            tb.markdown = "";
            doc_->undoStack()->push(new AddTextBoxCommand(doc_, tb));
            startEditingTextBox(tb.id);
        }
        e->accept();
        return;
    }
    else if (tool_ == Tool::Select) {
        const qint64 hit = hitTestTextBox(world);
        activeTextId_ = hit;
        if (hit >= 0) {
            int idx = doc_->textBoxIndexById(hit);
            dragStartRect_ = doc_->textBoxes()[idx].rectWorld;
            dragStartWorld_ = world;
            
            QPointF brView = worldToView(dragStartRect_.bottomRight());
            if (QLineF(e->position(), brView).length() < kHandleSizeView * 1.5) {
                isResizingText_ = true;
            } else {
                isDraggingText_ = true;
            }
            update(); e->accept();
        }
    }
}

void CanvasWidget::mouseMoveEvent(QMouseEvent *e)
{
    const QPointF world = viewToWorld(e->position());

    if (isPanning_) {
        panViewPx_ += (e->position() - lastPanViewPos_);
        lastPanViewPos_ = e->position();
        if (editor_ && editor_->isVisible()) startEditingTextBox(activeTextId_);
        update(); return;
    }

    if (isResizingText_ && activeTextId_ >= 0) {
        QPointF delta = world - dragStartWorld_;
        QRectF next = dragStartRect_;
        next.setRight(std::max(next.left() + 20/zoom_, dragStartRect_.right() + delta.x()));
        next.setBottom(std::max(next.top() + 20/zoom_, dragStartRect_.bottom() + delta.y()));
        doc_->setTextBoxRectById(activeTextId_, next);
        if (editor_ && editor_->isVisible()) {
            QRectF vr = QRectF(worldToView(next.topLeft()), worldToView(next.bottomRight())).normalized();
            editor_->setGeometry(vr.toRect());
        }
        update(); return;
    }

    if (isDraggingText_ && activeTextId_ >= 0) {
        doc_->setTextBoxRectById(activeTextId_, dragStartRect_.translated(world - dragStartWorld_));
        if (editor_ && editor_->isVisible()) startEditingTextBox(activeTextId_);
        update(); return;
    }

    if (tool_ == Tool::Pen && isDrawing_) appendStrokePoint(world, 1.0f);
    else if (tool_ == Tool::Eraser && (e->buttons() & Qt::LeftButton)) eraseAt(world, 10.0 / zoom_);
    update();
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent *e)
{
    Q_UNUSED(e);
    if (isResizingText_ || isDraggingText_) {
        if (doc_ && activeTextId_ >= 0) {
            int idx = doc_->textBoxIndexById(activeTextId_);
            if (idx >= 0 && doc_->textBoxes()[idx].rectWorld != dragStartRect_) {
                doc_->undoStack()->push(new SetTextBoxRectCommand(doc_, activeTextId_, dragStartRect_, doc_->textBoxes()[idx].rectWorld));
            }
        }
        isResizingText_ = isDraggingText_ = false;
    }
    isPanning_ = false;
    if (tool_ == Tool::Pen) endStroke();
    update();
}

void CanvasWidget::wheelEvent(QWheelEvent *e)
{
    if (e->modifiers().testFlag(Qt::ControlModifier)) {
        QPointF anchorWorld = viewToWorld(e->position());
        zoom_ = std::clamp(zoom_ * std::pow(1.15, e->angleDelta().y() / 120.0), 0.1, 12.0);
        panViewPx_ = e->position() - (anchorWorld * zoom_);
    } else {
        panViewPx_ += QPointF(e->angleDelta().x() / 4.0, e->angleDelta().y() / 4.0);
    }
    if (editor_ && editor_->isVisible()) startEditingTextBox(activeTextId_);
    update();
}

void CanvasWidget::keyPressEvent(QKeyEvent *e)
{
    if ((e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) && activeTextId_ >= 0) {
        if (!editor_ || !editor_->hasFocus()) {
            int idx = doc_->textBoxIndexById(activeTextId_);
            if (idx >= 0) {
                doc_->undoStack()->push(new RemoveTextBoxCommand(doc_, idx));
                activeTextId_ = -1; if (editor_) editor_->hide(); update();
            }
        }
    }
    QWidget::keyPressEvent(e);
}

void CanvasWidget::tabletEvent(QTabletEvent *e)
{
    const QPointF world = viewToWorld(e->position());
    const float pressure = std::clamp(static_cast<float>(e->pressure()), 0.0f, 1.0f);
    if (tool_ == Tool::Pen) {
        if (e->type() == QEvent::TabletPress) beginStroke(world, pressure);
        else if (e->type() == QEvent::TabletMove) appendStrokePoint(world, pressure);
        else if (e->type() == QEvent::TabletRelease) endStroke();
        e->accept(); return;
    }
    QWidget::tabletEvent(e);
}

void CanvasWidget::drawPages(QPainter &p) const
{
    constexpr double pageW = 595.0, pageH = 842.0, gap = 48.0;
    const QRectF worldView = viewToWorld(QRectF(QPointF(0, 0), QSizeF(size())));
    const double stride = pageH + gap;
    const int firstIdx = std::max(0, (int)std::floor(worldView.top() / stride));
    const int lastIdx = (int)std::ceil(worldView.bottom() / stride);
    p.fillRect(rect(), QColor(230, 230, 230));
    for (int i = firstIdx; i <= lastIdx; ++i) {
        const QRectF pageWorld(0.0, i * stride, pageW, pageH);
        const QRectF pageView(worldToView(pageWorld.topLeft()), worldToView(pageWorld.bottomRight()));
        p.fillRect(pageView.translated(3, 3), QColor(0, 0, 0, 20));
        p.fillRect(pageView, QColor(255, 255, 255));
        p.setPen(QPen(QColor(200, 200, 200), 1));
        p.drawRect(pageView);
    }
}

void CanvasWidget::drawStrokes(QPainter &p) const
{
    auto drawS = [&](const Stroke &s) {
        if (s.pts.size() < 2) return;
        QPen pen(s.color, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        if (s.isShape) {
            pen.setWidthF(s.baseWidthPoints * zoom_); p.setPen(pen);
            QDataStream ds(s.shapeParams); ds.setVersion(QDataStream::Qt_6_0);
            if (s.shapeType == "line") { QPointF a, b; ds >> a >> b; p.drawLine(worldToView(a), worldToView(b)); }
            else if (s.shapeType == "circle") { QPointF c; double r; ds >> c >> r; p.drawEllipse(worldToView(c), r*zoom_, r*zoom_); }
            else if (s.shapeType == "rect") { QRectF r; ds >> r; p.drawRect(QRectF(worldToView(r.topLeft()), worldToView(r.bottomRight())).normalized()); }
            return;
        }
        for (int i = 1; i < s.pts.size(); ++i) {
            pen.setWidthF(s.baseWidthPoints * s.pts[i].pressure * zoom_); p.setPen(pen);
            p.drawLine(worldToView(s.pts[i-1].worldPos), worldToView(s.pts[i].worldPos));
        }
    };
    if (doc_) for (const auto &s : doc_->strokes()) drawS(s);
    if (isDrawing_ && draft_.size() >= 2) {
        Stroke s; s.color = draftColor_; s.baseWidthPoints = draftBaseWidthPoints_;
        for (const auto &dp : draft_) s.pts.push_back(StrokePoint{dp.worldPos, dp.pressure, (int)dp.tMs});
        drawS(s);
    }
}

void CanvasWidget::drawTextBoxes(QPainter &p) const
{
    if (!doc_) return;
    for (const auto &tb : doc_->textBoxes()) {
        QRectF vr = QRectF(worldToView(tb.rectWorld.topLeft()), worldToView(tb.rectWorld.bottomRight())).normalized();
        p.setPen(QPen(tb.id == activeTextId_ ? Qt::blue : QColor(180, 180, 180), tb.id == activeTextId_ ? 2 : 1, tb.id == activeTextId_ ? Qt::DashLine : Qt::SolidLine));
        p.setBrush(QColor(255, 255, 255, 220));
        p.drawRoundedRect(vr, 4, 4);
        
        // Resize handle
        if (tool_ == Tool::Select && tb.id == activeTextId_) {
            p.setBrush(Qt::blue); p.setPen(Qt::NoPen);
            p.drawRect(vr.right() - 4, vr.bottom() - 4, 8, 8);
        }

        QTextDocument d; d.setMarkdown(tb.markdown); d.setTextWidth(vr.width() - 10);
        p.save(); p.translate(vr.topLeft() + QPointF(5, 5)); d.drawContents(&p); p.restore();
    }
}

void CanvasWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    if (viewMode_ == ViewMode::A4Notebook) drawPages(p);
    else p.fillRect(rect(), palette().color(QPalette::Base));
    drawStrokes(p); drawTextBoxes(p);
}

qint64 CanvasWidget::hitTestTextBox(const QPointF &worldPos) const {
    if (!doc_) return -1;
    for (int i = (int)doc_->textBoxes().size() - 1; i >= 0; --i) {
        if (doc_->textBoxes()[i].rectWorld.contains(worldPos)) return doc_->textBoxes()[i].id;
    }
    return -1;
}

void CanvasWidget::startEditingTextBox(qint64 id) {
    if (!doc_ || id < 0) return;
    activeTextId_ = id;
    int idx = doc_->textBoxIndexById(id); if (idx < 0) return;
    if (!editor_) {
        editor_ = new QPlainTextEdit(this);
        editor_->setStyleSheet("background: white; border: 1px solid #333;");
        editorCommitTimer_ = new QTimer(this); editorCommitTimer_->setSingleShot(true);
        connect(editor_, &QPlainTextEdit::textChanged, [this]() {
            if (activeTextId_ < 0) return;
            doc_->setTextBoxMarkdownById(activeTextId_, editor_->toPlainText());
            editorCommitTimer_->start(500);
        });
        connect(editorCommitTimer_, &QTimer::timeout, [this]() {
            if (activeTextId_ < 0) return;
            int i = doc_->textBoxIndexById(activeTextId_);
            doc_->undoStack()->push(new SetTextBoxMarkdownCommand(doc_, activeTextId_, doc_->textBoxes()[i].markdown, editor_->toPlainText()));
        });
    }
    const auto &tb = doc_->textBoxes()[idx];
    QRectF vr = QRectF(worldToView(tb.rectWorld.topLeft()), worldToView(tb.rectWorld.bottomRight())).normalized();
    editor_->setGeometry(vr.toRect());
    if (editor_->toPlainText() != tb.markdown) editor_->setPlainText(tb.markdown);
    editor_->show(); editor_->setFocus();
}

QRectF CanvasWidget::currentViewportWorld() const
{
    return viewToWorld(QRectF(QPointF(0, 0), QSizeF(size())));
}