#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QPointF>
#include <QRectF>
#include <QWidget>

class QPainter;

class Document;

class CanvasWidget : public QWidget {
  Q_OBJECT
 public:
  enum class Tool {
    Pen,
    Eraser,
    Select,
    Text,
  };

  enum class ViewMode {
    Infinite,
    A4Notebook,
  };

  explicit CanvasWidget(QWidget* parent = nullptr);

  void setDocument(Document* doc);
  Document* document() const { return doc_; }

  QRectF currentViewportWorld() const;

  void setTool(Tool tool);
  Tool tool() const { return tool_; }

  void setViewMode(ViewMode mode);
  ViewMode viewMode() const { return viewMode_; }

  void setPenColor(const QColor& c);
  QColor penColor() const { return penColor_; }

  void setPenWidthPoints(double w);
  double penWidthPoints() const { return penWidthPoints_; }

  void setSmartShapesEnabled(bool enabled);
  bool smartShapesEnabled() const { return smartShapesEnabled_; }

  // Font control methods
    void updateFontSize(int pointSize);
    void updateFontFamily(const QString &family);
    void setFontSize(int points); // Alias for consistency
    void setFontFamily(const QString &family); // Alias for consistency

 protected:
  void paintEvent(QPaintEvent* e) override;
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void mouseReleaseEvent(QMouseEvent* e) override;
  void wheelEvent(QWheelEvent* e) override;
  void tabletEvent(QTabletEvent* e) override;
  void keyPressEvent(QKeyEvent *e) override;

 private:
  struct DraftPoint {
    QPointF worldPos;
    float pressure = 1.0f;
    qint64 tMs = 0;
  };

  QPointF viewToWorld(const QPointF& viewPos) const;
  QPointF worldToView(const QPointF& worldPos) const;
  QRectF viewToWorld(const QRectF& viewRect) const;

  void beginStroke(const QPointF& worldPos, float pressure);
  void appendStrokePoint(const QPointF& worldPos, float pressure);
  void endStroke();
  void eraseAt(const QPointF& worldPos, double radiusWorld);

  void drawPages(QPainter& p) const;
  void drawStrokes(QPainter& p) const;
  void drawTextBoxes(QPainter& p) const;
  qint64 hitTestTextBox(const QPointF& worldPos) const;
  void startEditingTextBox(qint64 id);


  Document* doc_ = nullptr;

  Tool tool_ = Tool::Pen;
  ViewMode viewMode_ = ViewMode::Infinite;
  QColor penColor_ = QColor(20, 20, 20);
  QFont currentFont_ = QFont("Arial", 14); // Default font
  double penWidthPoints_ = 2.0;
  bool smartShapesEnabled_ = true;

  QVector<DraftPoint> draft_;
  QColor draftColor_;
  double draftBaseWidthPoints_ = 2.0;
  bool isDrawing_ = false;

  bool isPanning_ = false;
  QPointF lastPanViewPos_;

  double zoom_ = 1.0;
  QPointF panViewPx_ = {};

  bool isResizingText_ = false; // Add this line!

  // Text layer (MVP)
  qint64 activeTextId_ = -1;
  bool isDraggingText_ = false;
  QPointF dragStartWorld_;
  QRectF dragStartRect_;
  class QPlainTextEdit* editor_ = nullptr;
  class QTimer* editorCommitTimer_ = nullptr;

  QElapsedTimer timer_;
};
