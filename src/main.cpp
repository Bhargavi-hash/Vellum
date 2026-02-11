#include <QApplication>
#include <QIcon>
#include <QStyleFactory>

#include "app/MainWindow.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  QApplication::setApplicationName("Vellum");
  QApplication::setOrganizationName("Vellum");

  // Adwaita-ish minimal look without external deps.
  app.setStyle(QStyleFactory::create("Fusion"));
  app.setWindowIcon(QIcon::fromTheme("accessories-text-editor"));

  MainWindow w;
  w.show();
  return app.exec();
}

