#include "Document.h"

Document::Document(QObject* parent) : QObject(parent) {
  undo_.setUndoLimit(200);
}

void Document::clear() {
  undo_.clear();
  strokes_.clear();
  textBoxes_.clear();
  nextStrokeId_ = 1;
  nextTextBoxId_ = 1;
  emit changed();
}

void Document::setViewMode(ViewMode m) {
  if (viewMode_ == m) return;
  viewMode_ = m;
  emit viewModeChanged(viewMode_);
  emit changed();
}

int Document::insertStroke(int index, Stroke s) {
  if (index < 0 || index > strokes_.size()) index = strokes_.size();
  strokes_.insert(index, std::move(s));
  emit changed();
  return index;
}

Stroke Document::takeStrokeAt(int index) {
  if (index < 0 || index >= strokes_.size()) return Stroke{};
  Stroke s = strokes_.takeAt(index);
  emit changed();
  return s;
}

int Document::strokeIndexById(qint64 id) const {
  for (int i = 0; i < strokes_.size(); ++i) {
    if (strokes_[i].id == id) return i;
  }
  return -1;
}

void Document::setStrokeShapeById(qint64 id, bool isShape, const QString& type,
                                const QByteArray& params) {
  const int idx = strokeIndexById(id);
  if (idx < 0) return;
  strokes_[idx].isShape = isShape;
  strokes_[idx].shapeType = type;
  strokes_[idx].shapeParams = params;
  emit changed();
}

int Document::insertTextBox(int index, TextBox t) {
  if (index < 0 || index > textBoxes_.size()) index = textBoxes_.size();
  textBoxes_.insert(index, std::move(t));
  emit changed();
  return index;
}

TextBox Document::takeTextBoxAt(int index) {
  if (index < 0 || index >= textBoxes_.size()) return TextBox{};
  TextBox t = textBoxes_.takeAt(index);
  emit changed();
  return t;
}

int Document::textBoxIndexById(qint64 id) const {
  for (int i = 0; i < textBoxes_.size(); ++i) {
    if (textBoxes_[i].id == id) return i;
  }
  return -1;
}

void Document::setTextBoxRectById(qint64 id, const QRectF& r) {
  const int idx = textBoxIndexById(id);
  if (idx < 0) return;
  textBoxes_[idx].rectWorld = r;
  emit changed();
}

void Document::setTextBoxMarkdownById(qint64 id, const QString& md) {
  const int idx = textBoxIndexById(id);
  if (idx < 0) return;
  textBoxes_[idx].markdown = md;
  emit changed();
}

qint64 Document::nextStrokeId() {
  return nextStrokeId_++;
}

qint64 Document::nextTextBoxId() {
  return nextTextBoxId_++;
}

void Document::setNextIds(qint64 nextStrokeId, qint64 nextTextBoxId) {
  nextStrokeId_ = std::max<qint64>(1, nextStrokeId);
  nextTextBoxId_ = std::max<qint64>(1, nextTextBoxId);
}
