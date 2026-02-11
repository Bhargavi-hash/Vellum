#include "PdfExporter.h"

#include <QtMath>
#include <QDataStream>
#include <QFile>
#include <QPainter>
#include <QPdfWriter>
#include <QTextDocument>
#include <QPageSize>
#include <QPen>

#include "model/Document.h"

namespace {
constexpr double kA4W = 595.0;  // points
constexpr double kA4H = 842.0;
constexpr double kGap = 48.0;

QRectF docContentBoundsWorld(const Document& doc) {
  QRectF b;
  bool has = false;

  for (const auto& s : doc.strokes()) {
    if (s.pts.isEmpty()) continue;
    const QRectF sb = s.bounds();
    b = has ? (b | sb) : sb;
    has = true;
  }
  for (const auto& t : doc.textBoxes()) {
    b = has ? (b | t.rectWorld) : t.rectWorld;
    has = true;
  }
  if (!has) return QRectF(0, 0, 1, 1);
  return b;
}

void drawStrokeWorld(QPainter& p, const Stroke& s) {
  if (s.pts.size() < 2) return;

  QPen pen;
  pen.setColor(s.color);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);

  if (s.isShape && !s.shapeType.isEmpty()) {
    float avg = 0.0f;
    for (const auto& pt : s.pts) avg += pt.pressure;
    // Cast 1 to double to match s.pts.size() type
    avg /= std::max(1.0, static_cast<double>(s.pts.size()));
    pen.setWidthF(std::max(0.5, s.baseWidthPoints * static_cast<double>(avg)));
    p.setPen(pen);

    if (s.shapeType == "line") {
      QDataStream ds(s.shapeParams);
      ds.setVersion(QDataStream::Qt_6_0);
      QPointF a, b;
      ds >> a >> b;
      p.drawLine(a, b);
      return;
    }
    if (s.shapeType == "circle") {
      QDataStream ds(s.shapeParams);
      ds.setVersion(QDataStream::Qt_6_0);
      QPointF c;
      double r = 0;
      ds >> c >> r;
      p.drawEllipse(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r));
      return;
    }
    if (s.shapeType == "rect") {
      QDataStream ds(s.shapeParams);
      ds.setVersion(QDataStream::Qt_6_0);
      QRectF r;
      ds >> r;
      p.drawRect(r);
      return;
    }
  }

  for (int i = 1; i < s.pts.size(); ++i) {
    const auto& a = s.pts[i - 1];
    const auto& b = s.pts[i];
    const float pr = (a.pressure + b.pressure) * 0.5f;
    pen.setWidthF(std::max(0.5, s.baseWidthPoints * static_cast<double>(pr)));
    p.setPen(pen);
    p.drawLine(a.worldPos, b.worldPos);
  }
}

void drawTextBoxWorld(QPainter& p, const TextBox& tb) {
  QTextDocument doc;
  doc.setMarkdown(tb.markdown);
  doc.setTextWidth(std::max(1.0, tb.rectWorld.width() - 10.0));

  p.save();
  p.translate(tb.rectWorld.topLeft() + QPointF(5, 4));
  doc.drawContents(&p);
  p.restore();
}

}  // namespace

bool PdfExporter::exportToPdf(const QString& path, const Document& doc, const QRectF& viewportWorld,
                             QString* err) {
  QPdfWriter writer(path);
  writer.setResolution(72);  // 1 unit == 1 point
  writer.setPageSize(QPageSize(QPageSize::A4));

  QPainter p(&writer);
  if (!p.isActive()) {
    if (err) *err = "Failed to start PDF painter";
    return false;
  }
  p.setRenderHint(QPainter::Antialiasing, true);

  if (doc.viewMode() == Document::ViewMode::A4Notebook) {
    const QRectF content = docContentBoundsWorld(doc);
    const double stride = kA4H + kGap;
    const int lastPage = std::max(0, static_cast<int>(std::ceil(content.bottom() / stride)));

    for (int page = 0; page <= lastPage; ++page) {
      if (page != 0) writer.newPage();

      p.save();
      p.translate(0, -page * stride);

      // White page background.
      p.fillRect(QRectF(0, page * stride, kA4W, kA4H), Qt::white);

      for (const auto& s : doc.strokes()) drawStrokeWorld(p, s);
      for (const auto& t : doc.textBoxes()) drawTextBoxWorld(p, t);

      p.restore();
    }

    p.end();
    return true;
  }

  // Infinite mode: export current viewport to one page
  const QRectF vp = viewportWorld.isValid() ? viewportWorld : docContentBoundsWorld(doc);

  const double margin = 24.0;
  const QRectF pageRect(margin, margin, kA4W - 2 * margin, kA4H - 2 * margin);
  const double sx = pageRect.width() / std::max(1e-6, vp.width());
  const double sy = pageRect.height() / std::max(1e-6, vp.height());
  const double s = std::min(sx, sy);

  p.fillRect(QRectF(0, 0, kA4W, kA4H), Qt::white);
  p.save();
  p.translate(pageRect.center());
  p.scale(s, s);
  p.translate(-vp.center());

  for (const auto& s0 : doc.strokes()) drawStrokeWorld(p, s0);
  for (const auto& t : doc.textBoxes()) drawTextBoxWorld(p, t);

  p.restore();
  p.end();
  return true;
}