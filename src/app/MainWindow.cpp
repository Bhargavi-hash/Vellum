#include "MainWindow.h"

#include <QActionGroup>
#include <QFileDialog>
#include <QMenuBar>
#include <QToolBar>

#include "canvas/CanvasWidget.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("Vellum");

  auto* canvas = new CanvasWidget(this);
  setCentralWidget(canvas);

  // File menu (basic stubs for now; storage/export are added in later todos).
  auto* fileMenu = menuBar()->addMenu("&File");
  auto* actNew = fileMenu->addAction(QIcon::fromTheme("document-new"), "&New");
  auto* actOpen = fileMenu->addAction(QIcon::fromTheme("document-open"), "&Open…");
  auto* actSave = fileMenu->addAction(QIcon::fromTheme("document-save"), "&Save");
  auto* actSaveAs = fileMenu->addAction(QIcon::fromTheme("document-save-as"), "Save &As…");
  fileMenu->addSeparator();
  auto* actExportPdf = fileMenu->addAction(QIcon::fromTheme("document-export"), "Export &PDF…");
  fileMenu->addSeparator();
  fileMenu->addAction(QIcon::fromTheme("application-exit"), "Quit", this, &QWidget::close);

  actNew->setEnabled(false);
  actOpen->setEnabled(false);
  actSave->setEnabled(false);
  actSaveAs->setEnabled(false);
  actExportPdf->setEnabled(false);

  // Top toolbar (tool selection + view mode).
  auto* tb = addToolBar("Tools");
  tb->setMovable(false);

  auto* toolsGroup = new QActionGroup(this);
  toolsGroup->setExclusive(true);

  auto* actPen = tb->addAction(QIcon::fromTheme("draw-freehand"), "Pen");
  actPen->setCheckable(true);
  actPen->setChecked(true);
  toolsGroup->addAction(actPen);

  auto* actEraser = tb->addAction(QIcon::fromTheme("draw-eraser"), "Eraser");
  actEraser->setCheckable(true);
  toolsGroup->addAction(actEraser);

  auto* actSelect = tb->addAction(QIcon::fromTheme("edit-select"), "Select");
  actSelect->setCheckable(true);
  toolsGroup->addAction(actSelect);

  auto* actText = tb->addAction(QIcon::fromTheme("insert-text"), "Text");
  actText->setCheckable(true);
  toolsGroup->addAction(actText);

  tb->addSeparator();

  auto* actSmartShapes = tb->addAction(QIcon::fromTheme("draw-rectangle"), "Smart Shapes");
  actSmartShapes->setCheckable(true);
  actSmartShapes->setChecked(true);

  tb->addSeparator();

  auto* modeGroup = new QActionGroup(this);
  modeGroup->setExclusive(true);
  auto* actInfinite = tb->addAction("Infinite");
  actInfinite->setCheckable(true);
  actInfinite->setChecked(true);
  modeGroup->addAction(actInfinite);

  auto* actA4 = tb->addAction("A4");
  actA4->setCheckable(true);
  modeGroup->addAction(actA4);

  connect(actPen, &QAction::toggled, this, [canvas](bool on) {
    if (on) canvas->setTool(CanvasWidget::Tool::Pen);
  });
  connect(actEraser, &QAction::toggled, this, [canvas](bool on) {
    if (on) canvas->setTool(CanvasWidget::Tool::Eraser);
  });
  connect(actSelect, &QAction::toggled, this, [canvas](bool on) {
    if (on) canvas->setTool(CanvasWidget::Tool::Select);
  });
  connect(actText, &QAction::toggled, this, [canvas](bool on) {
    if (on) canvas->setTool(CanvasWidget::Tool::Text);
  });
  connect(actSmartShapes, &QAction::toggled, this,
          [canvas](bool on) { canvas->setSmartShapesEnabled(on); });
  connect(actInfinite, &QAction::toggled, this, [canvas](bool on) {
    if (on) canvas->setViewMode(CanvasWidget::ViewMode::Infinite);
  });
  connect(actA4, &QAction::toggled, this, [canvas](bool on) {
    if (on) canvas->setViewMode(CanvasWidget::ViewMode::A4Notebook);
  });

  resize(1100, 800);
}

