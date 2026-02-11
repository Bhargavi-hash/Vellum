#pragma once

#include <QUndoCommand>
#include <QString>
#include <QRectF>
#include <QByteArray>
#include "model/Document.h"

// --- Stroke Commands ---

class AddStrokeCommand : public QUndoCommand {
public:
    AddStrokeCommand(Document* doc, Stroke stroke, int index = -1);
    void undo() override;
    void redo() override;

private:
    Document* doc_;
    Stroke stroke_;
    int index_;
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
};

class SetStrokeShapeCommand : public QUndoCommand {
public:
    SetStrokeShapeCommand(Document* doc, qint64 id, bool isShape, const QString& type, const QByteArray& params);
    void undo() override;
    void redo() override;

private:
    Document* doc_;
    qint64 id_;
    bool isShapeBefore_, isShapeAfter_;
    QString typeBefore_, typeAfter_;
    QByteArray paramsBefore_, paramsAfter_;
};

// --- Text Box Commands ---

class AddTextBoxCommand : public QUndoCommand {
public:
    AddTextBoxCommand(Document* doc, TextBox box, int index = -1);
    void undo() override;
    void redo() override;

private:
    Document* doc_;
    TextBox box_;
    int index_;
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