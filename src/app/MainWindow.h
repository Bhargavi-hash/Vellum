#pragma once

#include <QMainWindow>
#include <QString>
#include <QFontComboBox>
#include <QSpinBox>

class QAction;
class CanvasWidget;
class Document;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);

 private:
  void newDocument();
  void openDocument();
  bool saveDocument();
  bool saveDocumentAs();
  void exportPdf();
  void createColorPalette();

  void setCurrentPath(const QString& path);
  void updateWindowTitle();

  CanvasWidget* canvas_ = nullptr;
  Document* doc_ = nullptr;

  QString currentPath_;

  QAction* actNew_ = nullptr;
  QAction* actOpen_ = nullptr;
  QAction* actSave_ = nullptr;
  QAction* actSaveAs_ = nullptr;
  QAction* actExportPdf_ = nullptr;

  QFontComboBox* fontCombo;
  QSpinBox* sizeSpin;
  void setupTextToolbar();
};
