#pragma once

#include <QString>

class Document;

class PdfExporter {
 public:
  // If doc is in A4 mode, exports pages that cover the document content.
  // If in infinite mode, exports current viewport bounds provided in world coords.
  static bool exportToPdf(const QString& path, const Document& doc, const QRectF& viewportWorld,
                          QString* err);
};
