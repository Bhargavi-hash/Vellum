#pragma once

#include <QByteArray>
#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QVector>

struct StrokePoint {
  QPointF worldPos;
  float pressure = 1.0f;  // 0..1
  qint64 tMs = 0;
};

struct Stroke {
  qint64 id = -1;
  QVector<StrokePoint> pts;
  QColor color = QColor(20, 20, 20);
  double baseWidthPoints = 2.0;

  // Shape snapping (filled in by smart-shapes todo).
  bool isShape = false;
  QString shapeType;       // e.g. "line", "circle", "rect"
  QByteArray shapeParams;  // binary blob (QDataStream)

  QRectF bounds() const;
};

