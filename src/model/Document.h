#pragma once

#include <QObject>
#include <QUndoStack>
#include <QVector>

#include "model/Stroke.h"
#include "model/TextBox.h"

class Document : public QObject {
  Q_OBJECT
 public:
  enum class ViewMode { Infinite, A4Notebook };

  explicit Document(QObject* parent = nullptr);

  void clear();

  ViewMode viewMode() const { return viewMode_; }
  void setViewMode(ViewMode m);

  const QVector<Stroke>& strokes() const { return strokes_; }
  const QVector<TextBox>& textBoxes() const { return textBoxes_; }

  QUndoStack* undoStack() { return &undo_; }

  // Internal mutation points used by undo commands.
  int insertStroke(int index, Stroke s);
  Stroke takeStrokeAt(int index);
  int strokeIndexById(qint64 id) const;

  int insertTextBox(int index, TextBox t);
  TextBox takeTextBoxAt(int index);
  int textBoxIndexById(qint64 id) const;
  void setTextBoxRectById(qint64 id, const QRectF& r);
  void setTextBoxMarkdownById(qint64 id, const QString& md);

  qint64 nextStrokeId();
  qint64 nextTextBoxId();

 signals:
  void changed();
  void viewModeChanged(Document::ViewMode);

 private:
  ViewMode viewMode_ = ViewMode::Infinite;
  QVector<Stroke> strokes_;
  QVector<TextBox> textBoxes_;
  QUndoStack undo_;
  qint64 nextStrokeId_ = 1;
  qint64 nextTextBoxId_ = 1;
};

