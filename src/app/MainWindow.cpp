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
#include <QGraphicsDropShadowEffect>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
  setWindowIcon(QIcon(":/assets/logo.png"));

  // 1. Updated Stylesheet for Floating Pill Aesthetic
  QString goodnotesStyle = R"(
        QMainWindow { background-color: #f2f2f2; }
        
        #DocumentBar {
            background-color: #3b6fb6;
            color: white;
            border: none;
            min-height: 45px;
        }

        /* The Floating Pill Bar */
        #FloatingToolbar {
    background-color: white;
    border: none; /* Removed the border */
    border-radius: 15px;
}

        /* Tool Buttons inside the pill */
        QToolButton {
            border: none;
            border-radius: 15px;
            padding: 5px;
            background: transparent;
        }
        QToolButton:hover { background-color: #f0f0f0; }
        QToolButton:checked { background-color: #dbeafe; border-radius: 15px; border: none; }

        /* Dropdown Menus */
        QMenu {
            background-color: white;
            border: 1px solid #d0d0d0;
            border-radius: 15px;
            padding: 2px;
        }

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

  // --- 2. DOCUMENT BAR (Top Blue Bar) ---
  auto *docBar = new QToolBar(this);
  docBar->setObjectName("DocumentBar");
  docBar->setMovable(false);
  addToolBar(Qt::TopToolBarArea, docBar);

  actNew_ = docBar->addAction(QIcon::fromTheme("document-new"), "New", this, &MainWindow::newDocument);
  actOpen_ = docBar->addAction(QIcon::fromTheme("document-open"), "Open", this, &MainWindow::openDocument);
  actSave_ = docBar->addAction(QIcon::fromTheme("document-save"), "Save", this, &MainWindow::saveDocument);

  auto *spacer = new QWidget();
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  docBar->addWidget(spacer);

  actExportPdf_ = docBar->addAction(QIcon::fromTheme("document-export"), "Export", this, &MainWindow::exportPdf);

  // --- 3. FLOATING TOOLBAR SETUP (The iPad Pill) ---
  floatingToolbar_ = new QWidget(this);

  // Create the shadow effect
  auto *shadow = new QGraphicsDropShadowEffect(this);
  shadow->setBlurRadius(30);             // How "soft" the shadow is
  shadow->setXOffset(0);                 // Center it horizontally
  shadow->setYOffset(4);                 // Push it down slightly
  shadow->setColor(QColor(0, 0, 0, 50)); // Subtle black with low alpha (transparency)

  // Apply it to the toolbar
  floatingToolbar_->setGraphicsEffect(shadow);
  floatingToolbar_->setObjectName("FloatingToolbar");

  floatingToolbar_->setFixedHeight(60);
  floatingToolbar_->setMinimumWidth(350);

  auto *pillLayout = new QHBoxLayout(floatingToolbar_);
  pillLayout->setContentsMargins(15, 5, 15, 5);
  pillLayout->setSpacing(10);

  auto *toolsGroup = new QActionGroup(this);
  toolsGroup->setExclusive(true);

  // Define Tools
  struct ToolDef
  {
    QString name;
    QString icon;
    CanvasWidget::Tool type;
  };
  QList<ToolDef> tools = {
      {"Pen", ":/pen.svg", CanvasWidget::Tool::Pen},
      {"Eraser", ":/eraser.svg", CanvasWidget::Tool::Eraser},
      {"Select", ":/select.svg", CanvasWidget::Tool::Select},
      {"Text", ":/text.svg", CanvasWidget::Tool::Text}};

  for (const auto &t : tools)
  {
    auto *btn = new QToolButton(floatingToolbar_);
    btn->setCheckable(true);
    btn->setIcon(QIcon(t.icon));
    btn->setToolTip(t.name);
    btn->setIconSize(QSize(32, 32));
    btn->setFixedSize(48, 48);

    btn->setAutoExclusive(true);
    if (t.type == CanvasWidget::Tool::Pen)
      btn->setChecked(true);

    pillLayout->addWidget(btn);
    // toolsGroup->addAction(btn->defaultAction()); // Grouping logic

    connect(btn, &QToolButton::toggled, this, [this, t](bool on)
            {
            if (on) canvas_->setTool(t.type); });
  }

  pillLayout->addSpacing(5);

  // --- 4. COLOR DROPDOWN ---
  auto *colorBtn = new QToolButton(floatingToolbar_);
  colorBtn->setIcon(QIcon(":/palette.svg")); // Ensure you have a palette icon
  colorBtn->setPopupMode(QToolButton::InstantPopup);
  colorBtn->setFixedSize(48, 48);
  colorBtn->setIconSize(QSize(32, 32));

  auto *colorMenu = new QMenu(colorBtn);
  // Create a container widget and a horizontal layout
  auto *colorContainer = new QWidget(colorMenu);
  auto *hLayout = new QHBoxLayout(colorContainer);
  hLayout->setContentsMargins(10, 5, 10, 5);
  hLayout->setSpacing(10);

  const QList<QColor> colors = {Qt::black, Qt::red, Qt::blue, QColor("#27ae60"), QColor("#f39c12")};

  for (const QColor &c : colors)
  {
    // Create a small button for each color instead of a menu action
    auto *cBtn = new QToolButton(colorContainer);
    cBtn->setFixedSize(32, 32);

    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(c);
    p.setPen(Qt::NoPen);
    p.drawEllipse(0, 0, 24, 24);
    p.end();

    cBtn->setIcon(QIcon(pix));
    cBtn->setIconSize(QSize(24, 24));
    cBtn->setStyleSheet("border: none; border-radius: 16px;");
    cBtn->setCursor(Qt::PointingHandCursor);

    connect(cBtn, &QToolButton::clicked, this, [this, c, colorMenu]()
            {
              canvas_->setPenColor(c);
              colorMenu->close(); // Close the menu after selection
            });

    hLayout->addWidget(cBtn);
  }

  // Wrap the container in a QWidgetAction
  auto *colorAction = new QWidgetAction(colorMenu);
  colorAction->setDefaultWidget(colorContainer);
  colorMenu->addAction(colorAction);
  colorBtn->setMenu(colorMenu);
  pillLayout->addWidget(colorBtn);

  // --- 5. TEXT OPTIONS DROPDOWN ---
  auto *textOptBtn = new QToolButton(floatingToolbar_);
  textOptBtn->setIcon(QIcon(":/fonts.svg"));
  textOptBtn->setPopupMode(QToolButton::InstantPopup);
  textOptBtn->setFixedSize(48, 48);
  textOptBtn->setIconSize(QSize(32, 32));

  auto *textMenu = new QMenu(textOptBtn);

  // Font Combo in Menu
  auto *fontAct = new QWidgetAction(textMenu);
  fontCombo = new QFontComboBox();
  fontAct->setDefaultWidget(fontCombo);
  textMenu->addAction(fontAct);

  // Size Spinbox in Menu
  auto *sizeAct = new QWidgetAction(textMenu);
  sizeSpin = new QSpinBox();
  sizeSpin->setRange(6, 99);
  sizeSpin->setSuffix(" pt");
  sizeAct->setDefaultWidget(sizeSpin);
  textMenu->addAction(sizeAct);

  textOptBtn->setMenu(textMenu);
  pillLayout->addWidget(textOptBtn);

  // Initial positioning
  floatingToolbar_->raise();
  updateWindowTitle();
  resize(1100, 800);
}

// Ensure the floating bar stays centered when window resizes
void MainWindow::resizeEvent(QResizeEvent *event)
{
  QMainWindow::resizeEvent(event);
  if (floatingToolbar_)
  {
    int x = (width() - floatingToolbar_->width()) / 2;
    floatingToolbar_->move(x, 70); // 70px down to clear the blue bar
  }
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
    connect(action, &QAction::triggered, [this, color]()
            { canvas_->setPenColor(color); });
  }

  targetBar->addSeparator();
  // Use a nice icon or symbol for the custom picker instead of text
  QAction *customColor = targetBar->addAction(QIcon::fromTheme("color-management"), "Custom");
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
