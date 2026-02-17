#include "MainWindow.h"

#include <QActionGroup>
#include <QFileDialog>
#include <QFileInfo>
#include <QColorDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QToolBar>

#include "canvas/CanvasWidget.h"
#include "model/Document.h"
#include "storage/SqliteStore.h"
#include "export/PdfExporter.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
  setWindowIcon(QIcon(":/assets/Vellum.png"));
  // 1. Apply macOS Global Styling (QSS)
  QString macStyle = R"(
    QMainWindow { background-color: #f6f6f6; }
    
    QToolBar {
        background-color: #f6f6f6;
        border: none;
        border-bottom: 1px solid #dcdcdc;
        spacing: 12px;
        padding: 6px;
    }

    /* Pill-shaped modern buttons */
    QToolButton {
        background-color: transparent;
        border-radius: 6px;
        padding: 5px;
    }
    
    QToolButton:checked {
        background-color: #d1d1d1;
        border: 1px solid #c0c0c0;
    }

    QToolButton:hover:!checked {
        background-color: #ececec;
    }

    /* Clean, rounded Input Fields */
    QSpinBox, QComboBox, QFontComboBox {
        background: white;
        border: 1px solid #dcdcdc;
        border-radius: 5px;
        padding: 3px 6px;
        font-size: 12px;
    }
)";
  setStyleSheet(macStyle);

  setWindowTitle("Vellum");

  // Core model and view setup
  doc_ = new Document(this);
  canvas_ = new CanvasWidget(this);
  canvas_->setDocument(doc_);
  setCentralWidget(canvas_);

  // --- MENU BAR SETUP ---
  auto *fileMenu = menuBar()->addMenu("&File");
  actNew_ = fileMenu->addAction(QIcon::fromTheme("document-new"), "&New", this, [this]() { newDocument(); });
  actOpen_ = fileMenu->addAction(QIcon::fromTheme("document-open"), "&Open…", this, [this]() { openDocument(); });
  actSave_ = fileMenu->addAction(QIcon::fromTheme("document-save"), "&Save", this, [this]() { saveDocument(); });
  actSaveAs_ = fileMenu->addAction(QIcon::fromTheme("document-save-as"), "Save &As…", this, [this]() { saveDocumentAs(); });
  fileMenu->addSeparator();
  actExportPdf_ = fileMenu->addAction(QIcon::fromTheme("document-export"), "Export &PDF…", this, [this]() { exportPdf(); });
  fileMenu->addSeparator();
  fileMenu->addAction(QIcon::fromTheme("application-exit"), "Quit", this, &QWidget::close);

  // --- UNIFIED TOOLBAR SETUP ---
  auto *tb = addToolBar("Main Toolbar");
  tb->setMovable(false);
  tb->setFloatable(false);
  tb->setToolButtonStyle(Qt::ToolButtonIconOnly);

  // Tool Selection Group
  auto *toolsGroup = new QActionGroup(this);
  toolsGroup->setExclusive(true);

  auto *actPen = tb->addAction(QIcon::fromTheme("draw-freehand"), "Pen");
  actPen->setCheckable(true);
  actPen->setChecked(true);
  toolsGroup->addAction(actPen);

  auto *actEraser = tb->addAction(QIcon::fromTheme("draw-eraser"), "Eraser");
  actEraser->setCheckable(true);
  toolsGroup->addAction(actEraser);

  auto *actSelect = tb->addAction(QIcon::fromTheme("edit-select"), "Select");
  actSelect->setCheckable(true);
  toolsGroup->addAction(actSelect);

  auto *actText = tb->addAction(QIcon::fromTheme("insert-text"), "Text");
  actText->setCheckable(true);
  toolsGroup->addAction(actText);

  tb->addSeparator();

  // Feature Toggles
  auto *actSmartShapes = tb->addAction(QIcon::fromTheme("draw-rectangle"), "Smart Shapes");
  actSmartShapes->setCheckable(true);
  actSmartShapes->setChecked(true);

  tb->addSeparator();

  // Text Property Widgets (Directly in Toolbar)
  fontCombo = new QFontComboBox(this);
  fontCombo->setMaximumWidth(160);
  tb->addWidget(fontCombo);

  sizeSpin = new QSpinBox(this);
  sizeSpin->setRange(6, 99);
  sizeSpin->setValue(12);
  sizeSpin->setSuffix(" pt");
  tb->addWidget(sizeSpin);

  tb->addSeparator();

  // View Mode Group
  auto *modeGroup = new QActionGroup(this);
  modeGroup->setExclusive(true);
  auto *actInfinite = tb->addAction("Infinite");
  actInfinite->setCheckable(true);
  actInfinite->setChecked(true);
  modeGroup->addAction(actInfinite);

  auto *actA4 = tb->addAction("A4");
  actA4->setCheckable(true);
  modeGroup->addAction(actA4);

  // Load secondary UI elements
  createColorPalette(); 

  // --- CONNECTIONS ---
  connect(actPen, &QAction::toggled, this, [this](bool on) { if (on) canvas_->setTool(CanvasWidget::Tool::Pen); });
  connect(actEraser, &QAction::toggled, this, [this](bool on) { if (on) canvas_->setTool(CanvasWidget::Tool::Eraser); });
  connect(actSelect, &QAction::toggled, this, [this](bool on) { if (on) canvas_->setTool(CanvasWidget::Tool::Select); });
  connect(actText, &QAction::toggled, this, [this](bool on) { if (on) canvas_->setTool(CanvasWidget::Tool::Text); });
  
  connect(actSmartShapes, &QAction::toggled, this, [this](bool on) { canvas_->setSmartShapesEnabled(on); });
  
  connect(actInfinite, &QAction::toggled, this, [this](bool on) { if (on) canvas_->setViewMode(CanvasWidget::ViewMode::Infinite); });
  connect(actA4, &QAction::toggled, this, [this](bool on) { if (on) canvas_->setViewMode(CanvasWidget::ViewMode::A4Notebook); });

  connect(fontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont &f) { canvas_->updateFontFamily(f.family()); });
  connect(sizeSpin, &QSpinBox::valueChanged, this, [this](int s) { canvas_->updateFontSize(s); });

  connect(doc_, &Document::changed, this, [this]() { updateWindowTitle(); });

  updateWindowTitle();
  resize(1100, 800);
}
void MainWindow::newDocument()
{
  doc_->clear();
  setCurrentPath(QString());
}

void MainWindow::createColorPalette()
{
  QToolBar *colorBar = addToolBar("Colors");
  const QList<QColor> colors = {
      Qt::black, Qt::red, Qt::blue,
      QColor("#27ae60"), // Emerald Green
      QColor("#f39c12")  // Orange
  };

  for (const QColor &color : colors)
  {
    // Create a 16x16 color block icon
    QPixmap pix(16, 16);
    pix.fill(color);

    QAction *action = colorBar->addAction(QIcon(pix), "");
    connect(action, &QAction::triggered, [this, color]()
            { canvas_->setPenColor(color); });
  }

  // Add a custom color picker button
  colorBar->addSeparator();
  QAction *customColor = colorBar->addAction("Custom");
  connect(customColor, &QAction::triggered, [this]()
          {
        QColor c = QColorDialog::getColor(Qt::black, this);
        if (c.isValid()) canvas_->setPenColor(c); });
}

void MainWindow::setupTextToolbar()
{
  QToolBar *textToolbar = addToolBar("Text Options");

  // 1. Font Family Dropdown
  fontCombo = new QFontComboBox(this);
  textToolbar->addWidget(fontCombo);

  // 2. Font Size Spinner
  sizeSpin = new QSpinBox(this);
  sizeSpin->setRange(6, 100);
  sizeSpin->setValue(12);
  sizeSpin->setSuffix(" pt");
  textToolbar->addWidget(sizeSpin);

  // --- Connections ---

  // When the font family changes
  connect(fontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont &f)
          { canvas_->updateFontFamily(f.family()); });

  // When the font size changes
  connect(sizeSpin, &QSpinBox::valueChanged, this, [this](int s)
          { canvas_->updateFontSize(s); });
}

void MainWindow::openDocument()
{
  const QString path = QFileDialog::getOpenFileName(this, "Open Vellum note", QString(),
                                                    "Vellum Notes (*.vellum);;All Files (*)");
  if (path.isEmpty())
    return;

  QString err;
  if (!SqliteStore::loadFromFile(path, doc_, &err))
  {
    QMessageBox::critical(this, "Open failed", err);
    return;
  }

  setCurrentPath(path);
}

bool MainWindow::saveDocument()
{
  if (currentPath_.isEmpty())
    return saveDocumentAs();

  QString err;
  if (!SqliteStore::saveToFile(currentPath_, *doc_, &err))
  {
    QMessageBox::critical(this, "Save failed", err);
    return false;
  }
  updateWindowTitle();
  return true;
}

bool MainWindow::saveDocumentAs()
{
  const QString path = QFileDialog::getSaveFileName(this, "Save Vellum note", currentPath_,
                                                    "Vellum Notes (*.vellum);;All Files (*)");
  if (path.isEmpty())
    return false;

  QString finalPath = path;
  if (!finalPath.endsWith(".vellum"))
    finalPath += ".vellum";

  QString err;
  if (!SqliteStore::saveToFile(finalPath, *doc_, &err))
  {
    QMessageBox::critical(this, "Save failed", err);
    return false;
  }
  setCurrentPath(finalPath);
  return true;
}

void MainWindow::exportPdf()
{
  const QString path = QFileDialog::getSaveFileName(this, "Export PDF", QString(),
                                                    "PDF (*.pdf);;All Files (*)");
  if (path.isEmpty())
    return;
  QString finalPath = path;
  if (!finalPath.endsWith(".pdf"))
    finalPath += ".pdf";

  // Export current viewport in infinite mode; ignored in A4 mode.
  // For now we approximate viewport as what the canvas currently shows in world coords.
  const QRectF viewportWorld = canvas_->currentViewportWorld();

  QString err;
  if (!PdfExporter::exportToPdf(finalPath, *doc_, viewportWorld, &err))
  {
    QMessageBox::critical(this, "Export failed", err);
    return;
  }
}

void MainWindow::setCurrentPath(const QString &path)
{
  currentPath_ = path;
  updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
  const QString name = currentPath_.isEmpty() ? "Untitled" : QFileInfo(currentPath_).fileName();
  setWindowTitle(QString("%1 — Vellum").arg(name));
}
