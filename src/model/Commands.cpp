#include "Commands.h"

#include <utility>

AddStrokeCommand::AddStrokeCommand(Document* doc, Stroke stroke, int index)
    : doc_(doc), stroke_(std::move(stroke)), index_(index) {
  setText("Add stroke");
}

void AddStrokeCommand::redo() {
  if (first_) first_ = false;
  index_ = doc_->insertStroke(index_, stroke_);
}

void AddStrokeCommand::undo() {
  stroke_ = doc_->takeStrokeAt(index_);
}

RemoveStrokeCommand::RemoveStrokeCommand(Document* doc, int index) : doc_(doc), index_(index) {
  setText("Remove stroke");
}

void RemoveStrokeCommand::redo() {
  if (first_) first_ = false;
  removed_ = doc_->takeStrokeAt(index_);
}

void RemoveStrokeCommand::undo() {
  doc_->insertStroke(index_, removed_);
}

AddTextBoxCommand::AddTextBoxCommand(Document* doc, TextBox box, int index)
    : doc_(doc), box_(std::move(box)), index_(index) {
  setText("Add text box");
}

void AddTextBoxCommand::redo() {
  if (first_) first_ = false;
  index_ = doc_->insertTextBox(index_, box_);
}

void AddTextBoxCommand::undo() {
  box_ = doc_->takeTextBoxAt(index_);
}

SetTextBoxRectCommand::SetTextBoxRectCommand(Document* doc, qint64 id, QRectF before, QRectF after)
    : doc_(doc), id_(id), before_(std::move(before)), after_(std::move(after)) {
  setText("Move/resize text box");
}

void SetTextBoxRectCommand::redo() {
  doc_->setTextBoxRectById(id_, after_);
}

void SetTextBoxRectCommand::undo() {
  doc_->setTextBoxRectById(id_, before_);
}

RemoveTextBoxCommand::RemoveTextBoxCommand(Document* doc, int index) : doc_(doc), index_(index) {
  setText("Remove text box");
}

void RemoveTextBoxCommand::redo() {
  if (first_) first_ = false;
  removed_ = doc_->takeTextBoxAt(index_);
}

void RemoveTextBoxCommand::undo() {
  doc_->insertTextBox(index_, removed_);
}

SetTextBoxMarkdownCommand::SetTextBoxMarkdownCommand(Document* doc, qint64 id, QString before,
                                                     QString after)
    : doc_(doc), id_(id), before_(std::move(before)), after_(std::move(after)) {
  setText("Edit text");
}

void SetTextBoxMarkdownCommand::redo() {
  doc_->setTextBoxMarkdownById(id_, after_);
}

void SetTextBoxMarkdownCommand::undo() {
  doc_->setTextBoxMarkdownById(id_, before_);
}

SetStrokeShapeCommand::SetStrokeShapeCommand(Document* doc, qint64 strokeId, bool isShape,
                                             QString shapeType, QByteArray shapeParams)
    : doc_(doc), id_(strokeId), afterIsShape_(isShape), afterType_(std::move(shapeType)),
      afterParams_(std::move(shapeParams)) {
  setText("Snap shape");

  const int idx = doc_->strokeIndexById(id_);
  if (idx >= 0) {
    const auto& s = doc_->strokes()[idx];
    beforeIsShape_ = s.isShape;
    beforeType_ = s.shapeType;
    beforeParams_ = s.shapeParams;
  }
}

void SetStrokeShapeCommand::redo() {
  doc_->setStrokeShapeById(id_, afterIsShape_, afterType_, afterParams_);
}

void SetStrokeShapeCommand::undo() {
  doc_->setStrokeShapeById(id_, beforeIsShape_, beforeType_, beforeParams_);
}
