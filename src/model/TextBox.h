#pragma once

#include <QRectF>
#include <QString>

struct TextBox {
  qint64 id = -1;
  QRectF rectWorld;
  QString markdown;
};

