#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QPointF>
#include <QVector>
#include <QWidget>

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

 protected:
  void paintEvent(QPaintEvent* e) override;
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void mouseReleaseEvent(QMouseEvent* e) override;
  void wheelEvent(QWheelEvent* e) override;
  void tabletEvent(QTabletEvent* e) override;

 private:
  struct StrokePoint {
    QPointF worldPos;
    float pressure = 1.0f;  // 0..1
    qint64 tMs = 0;
  };

  struct Stroke {
    QVector<StrokePoint> pts;
    QColor color;
    double baseWidthPoints = 2.0;
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

  Tool tool_ = Tool::Pen;
  ViewMode viewMode_ = ViewMode::Infinite;
  QColor penColor_ = QColor(20, 20, 20);
  double penWidthPoints_ = 2.0;
  bool smartShapesEnabled_ = true;

  QVector<Stroke> strokes_;
  Stroke current_;
  bool isDrawing_ = false;

  bool isPanning_ = false;
  QPointF lastPanViewPos_;

  double zoom_ = 1.0;       // viewPx per worldUnit (worldUnit==point for now)
  QPointF panViewPx_ = {};  // translation in view pixels

  QElapsedTimer timer_;
};

