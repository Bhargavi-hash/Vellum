#include "Commands.h"

AddStrokeCommand::AddStrokeCommand(Document* doc, Stroke stroke, int index)
    : doc_(doc), stroke_(std::move(stroke)), index_(index) {
  setText("Add stroke");
}

void AddStrokeCommand::redo() {
  if (first_) {
    // First redo comes from pushing onto the stack.
    first_ = false;
  }
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

