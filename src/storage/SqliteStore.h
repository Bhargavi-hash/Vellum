#pragma once

#include <QString>

class Document;

class SqliteStore {
 public:
  static bool saveToFile(const QString& path, const Document& doc, QString* err);
  static bool loadFromFile(const QString& path, Document* doc, QString* err);

 private:
  static bool ensureSchema(QString* err, const QString& connectionName);
};
