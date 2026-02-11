#pragma once

#include <QByteArray>
#include <QPainterPath>
#include <QVector>

#include "model/Stroke.h"

struct ShapeMatch {
  bool matched = false;
  QString type;      // "line" | "circle" | "rect"
  double score = 0;  // 0..1
  QByteArray params;
  QPainterPath path;  // perfect path in world coords
};

class ShapeRecognizer {
 public:
  static ShapeMatch recognize(const Stroke& stroke);
};
