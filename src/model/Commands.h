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

class RemoveTextBoxCommand : public QUndoCommand {
 public:
  RemoveTextBoxCommand(Document* doc, int index);
  void undo() override;
  void redo() override;

 private:
  Document* doc_;
  TextBox removed_;
  int index_;
  bool first_ = true;
};

class SetTextBoxMarkdownCommand : public QUndoCommand {
 public:
  SetTextBoxMarkdownCommand(Document* doc, qint64 id, QString before, QString after);
  void undo() override;
  void redo() override;

 private:
  Document* doc_;
  qint64 id_;
  QString before_;
  QString after_;
};

class SetStrokeShapeCommand : public QUndoCommand {
 public:
  SetStrokeShapeCommand(Document* doc, qint64 strokeId, bool isShape, QString shapeType,
                        QByteArray shapeParams);
  void undo() override;
  void redo() override;

 private:
  Document* doc_;
  qint64 id_;

  bool beforeIsShape_ = false;
  QString beforeType_;
  QByteArray beforeParams_;

  bool afterIsShape_ = false;
  QString afterType_;
  QByteArray afterParams_;
};
