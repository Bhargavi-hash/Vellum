#pragma once

#include <QMainWindow>
#include <QString>
#include <QFontComboBox>
#include <QSpinBox>
#include <QPainter>
#include <QResizeEvent>
#include <QToolButton>
#include <QMenu>
#include <QWidgetAction>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
// #include <QPaintEvent>
#include <QFrame>


class QAction;
class CanvasWidget;
class Document;
class QWidget;
class QSpinBox;
class QFontComboBox;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);

  protected:
    void resizeEvent(QResizeEvent *event) override;
    // void paintEvent(QPaintEvent *event) override;

 private:
  void newDocument();
  void openDocument();
  bool saveDocument();
  void renameDocument();
  bool saveDocumentAs();
  void exportPdf();
  void createColorPalette(QToolBar* targetBar);

  void setCurrentPath(const QString& path);
  void updateWindowTitle();

  CanvasWidget* canvas_ = nullptr;
  Document* doc_ = nullptr;

  QString currentPath_;

  QWidget* floatingToolbar_ = nullptr;

  QAction* actNew_ = nullptr;
  QAction* actOpen_ = nullptr;
  QAction* actSave_ = nullptr;
  QAction* actSaveAs_ = nullptr;
  QAction* actExportPdf_ = nullptr;

  QFontComboBox* fontCombo;
  QSpinBox* sizeSpin;

  QPushButton* titleBtn_ = nullptr; // Add this line
  void setupTextToolbar();
};
