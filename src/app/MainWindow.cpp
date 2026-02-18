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
  setWindowIcon(QIcon(":/assets/logo.png"));
  // 1. Apply macOS Global Styling (QSS)
 QString goodnotesStyle = R"(
    /* The very top document bar */
    #DocumentBar {
        background-color: #3b6fb6; /* The specific Goodnotes blue */
        color: white;
        border: none;
        min-height: 40px;
    }

    /* The main white toolbar */
    QToolBar#DrawingToolbar {
        background-color: #ffffff;
        border: none;
        border-bottom: 1px solid #e0e0e0; /* Very light separator */
        spacing: 15px;
        padding: 5px 10px;
    }

    /* Circular/Pill highlight for the active tool */
    QToolBar#DrawingToolbar QToolButton {
        background-color: transparent;
        border-radius: 15px; /* Makes it look like a circle */
        padding: 5px;
        color: #444;
    }

    /* The "light blue circle" highlight from your image */
    QToolBar#DrawingToolbar QToolButton:checked {
        background-color: #dbeafe; /* Light blue circle background */
        border: none;
    }

    QToolBar#DrawingToolbar QToolButton:hover:!checked {
        background-color: #f3f4f6;
    }

    /* Make the Canvas have zero border to merge with the white bar */
    #CanvasContainer {
        border: none;
        background-color: #ffffff;
    }
)";
  setStyleSheet(goodnotesStyle);
  setWindowTitle("Vellum");

  // Core model and view setup
  doc_ = new Document(this);
  canvas_ = new CanvasWidget(this);
  canvas_->setObjectName("CanvasContainer");
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

  // --- LAYERED TOOLBAR SETUP ---
  // 1. Top Document Bar (The Blue Layer)
  auto *docBar = new QToolBar(this);
  docBar->setObjectName("DocumentBar");
  docBar->setMovable(false);
  docBar->setFloatable(false);
  addToolBar(Qt::TopToolBarArea, docBar);

  // Add Document Title/Actions here
  docBar->addAction(actNew_);
  docBar->addAction(actOpen_);
  docBar->addAction(actSave_);
  
  // Spacer to push things to the right if needed
  auto* spacer = new QWidget();
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  docBar->addWidget(spacer);
  docBar->addAction(actExportPdf_);

  // 2. Drawing Toolbar (The White Layer)
  auto *tb = new QToolBar(this); // Using 'tb' to keep your existing connections working
  tb->setObjectName("DrawingToolbar");
  tb->setMovable(false);
  tb->setFloatable(false);
  tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
  tb->setIconSize(QSize(24, 24)); // Slightly larger icons for a mobile feel
  addToolBar(Qt::TopToolBarArea, tb);

  // Tool Selection Group
  auto *toolsGroup = new QActionGroup(this);
  toolsGroup->setExclusive(true);

  auto *actPen = tb->addAction(QIcon(":/pen.svg"), "Pen");
  actPen->setCheckable(true);
  actPen->setChecked(true);
  toolsGroup->addAction(actPen);

  auto *actEraser = tb->addAction(QIcon(":/eraser.svg"), "Eraser");
  actEraser->setCheckable(true);
  toolsGroup->addAction(actEraser);

  auto *actSelect = tb->addAction(QIcon(":/select.svg"), "Select");
  actSelect->setCheckable(true);
  toolsGroup->addAction(actSelect);

  auto *actText = tb->addAction(QIcon(":/text.svg"), "Text");
  actText->setCheckable(true);
  toolsGroup->addAction(actText);

  tb->addSeparator();

  // Feature Toggles
  auto *actSmartShapes = tb->addAction(QIcon(":/shapes.svg"), "Smart Shapes");
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
  createColorPalette(tb); 

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

void MainWindow::createColorPalette(QToolBar *targetBar) // Use the pointer we passed in
{
  targetBar->addSeparator();

  const QList<QColor> colors = {
      Qt::black, Qt::red, Qt::blue,
      QColor("#27ae60"), // Emerald Green
      QColor("#f39c12")  // Orange
  };

  for (const QColor &color : colors)
  {
    // Create a circular color icon to match the modern UI
    QPixmap pix(20, 20);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(0, 0, 20, 20);
    painter.end();

    // Add action directly to targetBar (DrawingToolbar)
    QAction *action = targetBar->addAction(QIcon(pix), "");
    connect(action, &QAction::triggered, [this, color]() { 
        canvas_->setPenColor(color); 
    });
}

  targetBar->addSeparator();
  // Use a nice icon or symbol for the custom picker instead of text
  QAction *customColor = targetBar->addAction(QIcon::fromTheme("color-management"), "Custom");
  connect(customColor, &QAction::triggered, [this]() {
        QColor c = QColorDialog::getColor(Qt::black, this);
        if (c.isValid()) canvas_->setPenColor(c); 
  });
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
