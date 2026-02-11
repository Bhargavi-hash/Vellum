#pragma once

#include <QString>
#include <QRectF> // Essential for QRectF to be recognized
#include "model/Document.h"

class PdfExporter {
 public:
  static bool exportToPdf(const QString& path, const Document& doc, 
                          const QRectF& viewportWorld, QString* err = nullptr);
};