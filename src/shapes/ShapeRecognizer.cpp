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
    double sum = 0;
    QVector<double> rs;
    rs.reserve(pts.size());
    for (const auto& p : pts) {
        double r = QLineF(c, p.worldPos).length();
        rs.push_back(r);
        sum += r;
    }
    double mean = sum / rs.size();
    double var = 0;
    for (double r : rs) {
        double d = r - mean;
        var += d * d;
    }
    if (stddevOut) *stddevOut = std::sqrt(var / rs.size());
    return mean;
}

static QRectF boundsOf(const QVector<StrokePoint>& pts) {
    if (pts.isEmpty()) return QRectF();
    QRectF r(pts[0].worldPos, QSizeF(0, 0));
    for (const auto& p : pts) r |= QRectF(p.worldPos, QSizeF(0, 0));
    return r;
}

static bool isClosedish(const QVector<StrokePoint>& pts, double diag) {
    if (pts.size() < 6) return false;
    // Closing threshold: within 25% of the bounding box diagonal
    return QLineF(pts.front().worldPos, pts.back().worldPos).length() < (diag * 0.25);
}

// --- Refined Matchers ---

static ShapeMatch matchLine(const Stroke& s) {
    ShapeMatch m;
    if (s.pts.size() < 2) return m;

    QPointF p0 = s.pts.front().worldPos;
    QPointF p1 = s.pts.back().worldPos;
    QLineF ideal(p0, p1);
    double len = ideal.length();
    if (len < 15.0) return m;

    double totalErr = 0;
    for (const auto& p : s.pts) {
        // Distance from point to the finite line segment
        QPointF v = p1 - p0;
        QPointF w = p.worldPos - p0;
        double t = std::clamp(QPointF::dotProduct(w, v) / QPointF::dotProduct(v, v), 0.0, 1.0);
        QPointF projection = p0 + t * v;
        totalErr += QLineF(p.worldPos, projection).length();
    }

    double avgErr = totalErr / s.pts.size();
    // Tolerance: 3% of length
    if (avgErr < len * 0.03) {
        m.matched = true;
        m.type = "line";
        m.score = std::clamp(1.0 - (avgErr / (len * 0.1)), 0.0, 1.0);
        QDataStream ds(&m.params, QIODevice::WriteOnly);
        ds << p0 << p1;
    }
    return m;
}

static ShapeMatch matchCircle(const Stroke& s) {
    ShapeMatch m;
    if (s.pts.size() < 12) return m;
    
    QRectF b = boundsOf(s.pts);
    double diag = QLineF(b.topLeft(), b.bottomRight()).length();
    if (!isClosedish(s.pts, diag)) return m;

    QPointF c = centroid(s.pts);
    double stddev = 0;
    double r = meanRadius(s.pts, c, &stddev);

    // relStd measures "roundness". 0.1 is quite loose, 0.05 is tight.
    double relStd = stddev / r;
    if (relStd < 0.12) {
        m.matched = true;
        m.type = "circle";
        m.score = std::clamp(1.0 - relStd, 0.0, 1.0);
        QDataStream ds(&m.params, QIODevice::WriteOnly);
        ds << c << r;
    }
    return m;
}

static ShapeMatch matchRect(const Stroke& s) {
    ShapeMatch m;
    if (s.pts.size() < 12) return m;

    QRectF b = boundsOf(s.pts);
    double diag = QLineF(b.topLeft(), b.bottomRight()).length();
    if (!isClosedish(s.pts, diag)) return m;

    int hits = 0;
    double tol = diag * 0.05; // 5% of diagonal as snap tolerance
    for (const auto& p : s.pts) {
        double dx = std::min(std::abs(p.worldPos.x() - b.left()), std::abs(p.worldPos.x() - b.right()));
        double dy = std::min(std::abs(p.worldPos.y() - b.top()), std::abs(p.worldPos.y() - b.bottom()));
        if (std::min(dx, dy) < tol) hits++;
    }

    double ratio = (double)hits / s.pts.size();
    if (ratio > 0.7) {
        m.matched = true;
        m.type = "rect";
        m.score = ratio;
        QDataStream ds(&m.params, QIODevice::WriteOnly);
        ds << b;
    }
    return m;
}

ShapeMatch ShapeRecognizer::recognize(const Stroke& stroke) {
    ShapeMatch best;
    auto consider = [&](ShapeMatch m) {
        if (m.matched && (!best.matched || m.score > best.score)) best = m;
    };

    consider(matchLine(stroke));
    consider(matchCircle(stroke));
    consider(matchRect(stroke));
    return best;
}