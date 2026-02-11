#include "Commands.h"

// --- Stroke Commands ---

AddStrokeCommand::AddStrokeCommand(Document* doc, Stroke stroke, int index)
    : doc_(doc), stroke_(std::move(stroke)), index_(index) {
    setText("Add stroke");
}

void AddStrokeCommand::redo() {
    index_ = doc_->insertStroke(index_, stroke_);
}

void AddStrokeCommand::undo() {
    stroke_ = doc_->takeStrokeAt(index_);
}

RemoveStrokeCommand::RemoveStrokeCommand(Document* doc, int index) 
    : doc_(doc), index_(index) {
    setText("Remove stroke");
}

void RemoveStrokeCommand::redo() {
    removed_ = doc_->takeStrokeAt(index_);
}

void RemoveStrokeCommand::undo() {
    doc_->insertStroke(index_, removed_);
}

SetStrokeShapeCommand::SetStrokeShapeCommand(Document* doc, qint64 id, bool isShape, const QString& type, const QByteArray& params)
    : doc_(doc), id_(id), isShapeAfter_(isShape), typeAfter_(type), paramsAfter_(params) {
    setText("Recognize Shape");
    
    // Capture the state before the transformation
    int idx = doc_->strokeIndexById(id);
    if (idx >= 0) {
        const auto& s = doc_->strokes()[idx];
        isShapeBefore_ = s.isShape;
        typeBefore_ = s.shapeType;
        paramsBefore_ = s.shapeParams;
    }
}

void SetStrokeShapeCommand::redo() {
    // Corrected to use the existing member function name
    doc_->setStrokeShapeById(id_, isShapeAfter_, typeAfter_, paramsAfter_);
}

void SetStrokeShapeCommand::undo() {
    // Corrected to use the existing member function name
    doc_->setStrokeShapeById(id_, isShapeBefore_, typeBefore_, paramsBefore_);
}

// --- Text Box Commands ---

AddTextBoxCommand::AddTextBoxCommand(Document* doc, TextBox box, int index)
    : doc_(doc), box_(std::move(box)), index_(index) {
    setText("Add text box");
}

void AddTextBoxCommand::redo() {
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

RemoveTextBoxCommand::RemoveTextBoxCommand(Document* doc, int index) 
    : doc_(doc), index_(index) {
    setText("Remove text box");
}

void RemoveTextBoxCommand::redo() {
    removed_ = doc_->takeTextBoxAt(index_);
}

void RemoveTextBoxCommand::undo() {
    doc_->insertTextBox(index_, removed_);
}

SetTextBoxMarkdownCommand::SetTextBoxMarkdownCommand(Document* doc, qint64 id, QString before, QString after)
    : doc_(doc), id_(id), before_(std::move(before)), after_(std::move(after)) {
    setText("Edit text");
}

void SetTextBoxMarkdownCommand::redo() {
    doc_->setTextBoxMarkdownById(id_, after_);
}

void SetTextBoxMarkdownCommand::undo() {
    doc_->setTextBoxMarkdownById(id_, before_);
}