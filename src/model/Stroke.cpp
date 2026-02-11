#include "Stroke.h"

QRectF Stroke::bounds() const {
  if (pts.isEmpty()) return QRectF();
  QRectF r(pts[0].worldPos, QSizeF(0, 0));
  for (const auto& p : pts) r |= QRectF(p.worldPos, QSizeF(0, 0));
  return r;
}

