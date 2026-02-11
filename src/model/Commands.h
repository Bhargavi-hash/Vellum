#pragma once

#include <QUndoCommand>

#include "model/Document.h"

class AddStrokeCommand : public QUndoCommand {
 public:
  AddStrokeCommand(Document* doc, Stroke stroke, int index = -1);
  void undo() override;
  void redo() override;

 private:
  Document* doc_;
  Stroke stroke_;
  int index_;
  bool first_ = true;
};

class RemoveStrokeCommand : public QUndoCommand {
 public:
  RemoveStrokeCommand(Document* doc, int index);
  void undo() override;
  void redo() override;

 private:
  Document* doc_;
  Stroke removed_;
  int index_;
  bool first_ = true;
};

class AddTextBoxCommand : public QUndoCommand {
 public:
  AddTextBoxCommand(Document* doc, TextBox box, int index = -1);
  void undo() override;
  void redo() override;

 private:
  Document* doc_;
  TextBox box_;
  int index_;
  bool first_ = true;
};

class SetTextBoxRectCommand : public QUndoCommand {
 public:
  SetTextBoxRectCommand(Document* doc, qint64 id, QRectF before, QRectF after);
  void undo() override;
  void redo() override;

 private:
  Document* doc_;
  qint64 id_;
  QRectF before_;
  QRectF after_;
};

