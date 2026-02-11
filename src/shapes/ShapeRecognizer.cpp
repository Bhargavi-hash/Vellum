#include "ShapeRecognizer.h"

#include <QtMath>
#include <QDataStream>
#include <QLineF>
#include <QIODevice>
#include <QPainterPath>
#include <algorithm>

// --- Helper Functions ---

static QPointF centroid(const QVector<StrokePoint>& pts) {
  QPointF c(0, 0);
  if (pts.isEmpty()) return c;
  for (const auto& p : pts) c += p.worldPos;
  return c / pts.size();
}

static double meanRadius(const QVector<StrokePoint>& pts, const QPointF& c, double* stddevOut) {
  if (pts.isEmpty()) {
    if (stddevOut) *stddevOut = 0;
    return 0;
  }
  QVector<double> rs;
  rs.reserve(pts.size());
  double sum = 0;
  for (const auto& p : pts) {
    const double r = QLineF(c, p.worldPos).length();
    rs.push_back(r);
    sum += r;
  }
  const double mean = sum / rs.size();
  double var = 0;
  for (double r : rs) {
    const double d = r - mean;
    var += d * d;
  }
  var /= rs.size();
  if (stddevOut) *stddevOut = std::sqrt(var);
  return mean;
}

static QRectF boundsOf(const QVector<StrokePoint>& pts) {
  if (pts.isEmpty()) return QRectF();
  QRectF r(pts[0].worldPos, QSizeF(0, 0));
  for (const auto& p : pts) r |= QRectF(p.worldPos, QSizeF(0, 0));
  return r;
}

static bool isClosedish(const QVector<StrokePoint>& pts, double eps) {
  if (pts.size() < 6) return false;
  return QLineF(pts.front().worldPos, pts.back().worldPos).length() <= eps;
}

static double lineFitError(const QVector<StrokePoint>& pts, const QLineF& line) {
  if (pts.size() < 2) return 1e9;
  const QPointF a = line.p1();
  const QPointF b = line.p2();
  const QPointF ab = b - a;
  const double ab2 = QPointF::dotProduct(ab, ab);
  if (ab2 <= 1e-9) return 1e9;

  double sum = 0;
  for (const auto& p : pts) {
    const QPointF ap = p.worldPos - a;
    const double t = QPointF::dotProduct(ap, ab) / ab2;
    const QPointF proj = a + ab * t;
    sum += QLineF(p.worldPos, proj).length();
  }
  return sum / pts.size();
}

// --- Specific Shape Matchers ---

static ShapeMatch matchLine(const Stroke& s) {
  ShapeMatch m;
  if (s.pts.size() < 2) return m;

  const QPointF p0 = s.pts.front().worldPos;
  const QPointF p1 = s.pts.back().worldPos;
  const double len = QLineF(p0, p1).length();
  if (len < 10.0) return m;

  const QLineF line(p0, p1);
  const double err = lineFitError(s.pts, line);

  const double normErr = err / std::max(1.0, len);
  if (normErr > 0.02) return m; 

  m.matched = true;
  m.type = "line";
  m.score = std::clamp(1.0 - normErr * 30.0, 0.0, 1.0);

  QPainterPath path;
  path.moveTo(p0);
  path.lineTo(p1);
  m.path = path;

  QByteArray blob;
  QDataStream ds(&blob, QIODevice::WriteOnly);
  ds.setVersion(QDataStream::Qt_6_0);
  ds << p0 << p1;
  m.params = blob;
  return m;
}

static ShapeMatch matchCircle(const Stroke& s) {
  ShapeMatch m;
  if (s.pts.size() < 10) return m;

  const QRectF b = boundsOf(s.pts);
  if (b.width() < 20.0 || b.height() < 20.0) return m;

  const QPointF c = centroid(s.pts);
  double stddev = 0;
  const double r = meanRadius(s.pts, c, &stddev);
  if (r <= 0) return m;

  const double aspect = b.width() / std::max(1e-6, b.height());
  if (aspect < 0.75 || aspect > 1.33) return m;

  const double closeEps = std::max(12.0, r * 0.20);
  if (!isClosedish(s.pts, closeEps)) return m;

  const double relStd = stddev / std::max(1e-6, r);
  if (relStd > 0.12) return m;

  m.matched = true;
  m.type = "circle";
  m.score = std::clamp(1.0 - relStd * 6.0, 0.0, 1.0);

  QPainterPath path;
  path.addEllipse(c, r, r);
  m.path = path;

  QByteArray blob;
  QDataStream ds(&blob, QIODevice::WriteOnly);
  ds.setVersion(QDataStream::Qt_6_0);
  ds << c << r;
  m.params = blob;
  return m;
}

static ShapeMatch matchRect(const Stroke& s) {
  ShapeMatch m;
  if (s.pts.size() < 10) return m;

  const QRectF b = boundsOf(s.pts);
  if (b.width() < 30.0 || b.height() < 30.0) return m;

  const double closeEps = std::max(12.0, std::min(b.width(), b.height()) * 0.15);
  if (!isClosedish(s.pts, closeEps)) return m;

  const double tol = std::min(b.width(), b.height()) * 0.06;
  int nearEdge = 0;
  for (const auto& p : s.pts) {
    const double dxL = std::abs(p.worldPos.x() - b.left());
    const double dxR = std::abs(p.worldPos.x() - b.right());
    const double dyT = std::abs(p.worldPos.y() - b.top());
    const double dyB = std::abs(p.worldPos.y() - b.bottom());
    const double d = std::min({dxL, dxR, dyT, dyB});
    if (d <= tol) nearEdge++;
  }
  const double frac = nearEdge / std::max(1.0, static_cast<double>(s.pts.size()));
  if (frac < 0.75) return m;

  m.matched = true;
  m.type = "rect";
  m.score = std::clamp((frac - 0.75) / 0.25, 0.0, 1.0);

  QPainterPath path;
  path.addRect(b);
  m.path = path;

  QByteArray blob;
  QDataStream ds(&blob, QIODevice::WriteOnly);
  ds.setVersion(QDataStream::Qt_6_0);
  ds << b;
  m.params = blob;
  return m;
}

// --- Public Interface ---

ShapeMatch ShapeRecognizer::recognize(const Stroke& stroke) {
  ShapeMatch best;

  auto consider = [&](const ShapeMatch& m) {
    if (!m.matched) return;
    if (!best.matched || m.score > best.score) best = m;
  };

  consider(matchLine(stroke));
  consider(matchCircle(stroke));
  consider(matchRect(stroke));

  return best;
}