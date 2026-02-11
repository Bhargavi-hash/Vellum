#include "SqliteStore.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

#include "model/Document.h"

static QString lastSqlError(const QSqlDatabase& db) {
  return db.lastError().text();
}

static bool execOrErr(QSqlQuery& q, QString* err) {
  if (q.exec()) return true;
  if (err) *err = q.lastError().text();
  return false;
}

static bool beginTx(QSqlDatabase& db, QString* err) {
  if (db.transaction()) return true;
  if (err) *err = lastSqlError(db);
  return false;
}

static bool commitTx(QSqlDatabase& db, QString* err) {
  if (db.commit()) return true;
  if (err) *err = lastSqlError(db);
  return false;
}

static bool rollbackTx(QSqlDatabase& db) {
  return db.rollback();
}

static int packColorRgba(const QColor& c) {
  return (c.alpha() << 24) | (c.red() << 16) | (c.green() << 8) | (c.blue());
}

static QColor unpackColorRgba(int v) {
  const int a = (v >> 24) & 0xFF;
  const int r = (v >> 16) & 0xFF;
  const int g = (v >> 8) & 0xFF;
  const int b = v & 0xFF;
  return QColor(r, g, b, a);
}

bool SqliteStore::ensureSchema(QString* err, const QString& connectionName) {
  auto db = QSqlDatabase::database(connectionName);
  QSqlQuery q(db);

  // Fix: Separate prepare from execOrErr
  if (!q.prepare("PRAGMA foreign_keys=ON") || !execOrErr(q, err)) return false;

  // meta table
  if (!q.prepare("CREATE TABLE IF NOT EXISTS meta("
                 "  key TEXT PRIMARY KEY,"
                 "  value TEXT"
                 ")") || !execOrErr(q, err)) return false;

  // strokes table
  if (!q.prepare("CREATE TABLE IF NOT EXISTS strokes("
                 "  id INTEGER PRIMARY KEY,"
                 "  tool TEXT,"
                 "  color_rgba INTEGER,"
                 "  base_width REAL,"
                 "  is_shape INTEGER,"
                 "  shape_type TEXT,"
                 "  shape_params BLOB,"
                 "  created_at INTEGER"
                 ")") || !execOrErr(q, err)) return false;

  // stroke points table
  if (!q.prepare("CREATE TABLE IF NOT EXISTS stroke_points("
                 "  stroke_id INTEGER NOT NULL,"
                 "  seq INTEGER NOT NULL,"
                 "  x REAL NOT NULL,"
                 "  y REAL NOT NULL,"
                 "  pressure REAL NOT NULL,"
                 "  t INTEGER NOT NULL,"
                 "  PRIMARY KEY(stroke_id, seq),"
                 "  FOREIGN KEY(stroke_id) REFERENCES strokes(id) ON DELETE CASCADE"
                 ")") || !execOrErr(q, err)) return false;

  if (!q.prepare("CREATE INDEX IF NOT EXISTS idx_stroke_points_sid ON stroke_points(stroke_id)") || 
      !execOrErr(q, err)) return false;

  // text boxes table
  if (!q.prepare("CREATE TABLE IF NOT EXISTS text_boxes("
                 "  id INTEGER PRIMARY KEY,"
                 "  x REAL NOT NULL,"
                 "  y REAL NOT NULL,"
                 "  w REAL NOT NULL,"
                 "  h REAL NOT NULL,"
                 "  markdown TEXT,"
                 "  created_at INTEGER,"
                 "  updated_at INTEGER"
                 ")") || !execOrErr(q, err)) return false;

  // pages table
  if (!q.prepare("CREATE TABLE IF NOT EXISTS pages("
                 "  id INTEGER PRIMARY KEY,"
                 "  page_index INTEGER,"
                 "  y_offset REAL"
                 ")") || !execOrErr(q, err)) return false;

  return true;
}

bool SqliteStore::saveToFile(const QString& path, const Document& doc, QString* err) {
  const QString conn = QString("vellum_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
  {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", conn);
    db.setDatabaseName(path);
    if (!db.open()) {
      if (err) *err = db.lastError().text();
      QSqlDatabase::removeDatabase(conn);
      return false;
    }

    if (!beginTx(db, err)) {
      db.close();
      QSqlDatabase::removeDatabase(conn);
      return false;
    }

    if (!ensureSchema(err, conn)) {
      rollbackTx(db);
      db.close();
      QSqlDatabase::removeDatabase(conn);
      return false;
    }

    QSqlQuery q(db);

    // Fix: Clean DELETE logic
    auto clearTable = [&](const QString& tableName) {
        if (!q.prepare(QString("DELETE FROM %1").arg(tableName))) return false;
        return execOrErr(q, err);
    };

    if (!clearTable("stroke_points") || !clearTable("strokes") || 
        !clearTable("text_boxes") || !clearTable("pages")) {
      rollbackTx(db);
      db.close();
      QSqlDatabase::removeDatabase(conn);
      return false;
    }

    // meta
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QString viewMode = (doc.viewMode() == Document::ViewMode::A4Notebook) ? "a4" : "infinite";

    if (!q.prepare("INSERT OR REPLACE INTO meta(key,value) VALUES(?,?)")) {
      rollbackTx(db);
      db.close();
      QSqlDatabase::removeDatabase(conn);
      return false;
    }

    auto putMeta = [&](const QString& key, const QString& value) {
      q.addBindValue(key);
      q.addBindValue(value);
      bool ok = execOrErr(q, err);
      // Re-prepare for next call since addBindValue consumes the bound state on some drivers
      q.prepare("INSERT OR REPLACE INTO meta(key,value) VALUES(?,?)"); 
      return ok;
    };

    if (!putMeta("doc_version", "1") || !putMeta("view_mode", viewMode) ||
        !putMeta("modified_at", QString::number(now))) {
      rollbackTx(db);
      db.close();
      QSqlDatabase::removeDatabase(conn);
      return false;
    }

    // strokes & points
    QSqlQuery insStroke(db);
    insStroke.prepare("INSERT INTO strokes(id,tool,color_rgba,base_width,is_shape,shape_type,shape_params,created_at) VALUES(?,?,?,?,?,?,?,?)");

    QSqlQuery insPt(db);
    insPt.prepare("INSERT INTO stroke_points(stroke_id,seq,x,y,pressure,t) VALUES(?,?,?,?,?,?)");

    for (const auto& s : doc.strokes()) {
      insStroke.addBindValue(s.id);
      insStroke.addBindValue(QStringLiteral("pen"));
      insStroke.addBindValue(packColorRgba(s.color));
      insStroke.addBindValue(s.baseWidthPoints);
      insStroke.addBindValue(s.isShape ? 1 : 0);
      insStroke.addBindValue(s.shapeType);
      insStroke.addBindValue(s.shapeParams);
      insStroke.addBindValue(now);
      if (!execOrErr(insStroke, err)) {
        rollbackTx(db);
        db.close();
        QSqlDatabase::removeDatabase(conn);
        return false;
      }

      for (int i = 0; i < s.pts.size(); ++i) {
        const auto& p = s.pts[i];
        insPt.addBindValue(s.id);
        insPt.addBindValue(i);
        insPt.addBindValue(p.worldPos.x());
        insPt.addBindValue(p.worldPos.y());
        insPt.addBindValue(p.pressure);
        insPt.addBindValue(p.tMs);
        if (!execOrErr(insPt, err)) {
          rollbackTx(db);
          db.close();
          QSqlDatabase::removeDatabase(conn);
          return false;
        }
      }
    }

    // text boxes
    QSqlQuery insText(db);
    insText.prepare("INSERT INTO text_boxes(id,x,y,w,h,markdown,created_at,updated_at) VALUES(?,?,?,?,?,?,?,?)");

    for (const auto& t : doc.textBoxes()) {
      insText.addBindValue(t.id);
      insText.addBindValue(t.rectWorld.x());
      insText.addBindValue(t.rectWorld.y());
      insText.addBindValue(t.rectWorld.width());
      insText.addBindValue(t.rectWorld.height());
      insText.addBindValue(t.markdown);
      insText.addBindValue(now);
      insText.addBindValue(now);
      if (!execOrErr(insText, err)) {
        rollbackTx(db);
        db.close();
        QSqlDatabase::removeDatabase(conn);
        return false;
      }
    }

    if (!commitTx(db, err)) {
      rollbackTx(db);
      db.close();
      QSqlDatabase::removeDatabase(conn);
      return false;
    }

    db.close();
  }
  QSqlDatabase::removeDatabase(conn);
  return true;
}

bool SqliteStore::loadFromFile(const QString& path, Document* doc, QString* err) {
  if (!doc) {
    if (err) *err = "Document is null";
    return false;
  }

  const QString conn = QString("vellum_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
  qint64 maxStrokeId = 0;
  qint64 maxTextId = 0;

  {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", conn);
    db.setDatabaseName(path);
    if (!db.open()) {
      if (err) *err = db.lastError().text();
      QSqlDatabase::removeDatabase(conn);
      return false;
    }

    if (!ensureSchema(err, conn)) {
      db.close();
      QSqlDatabase::removeDatabase(conn);
      return false;
    }

    // meta view mode
    QString viewMode = "infinite";
    {
      QSqlQuery q(db);
      if (q.prepare("SELECT value FROM meta WHERE key='view_mode'") && execOrErr(q, err)) {
          if (q.next()) viewMode = q.value(0).toString();
      }
    }

    doc->clear();
    doc->setViewMode(viewMode == "a4" ? Document::ViewMode::A4Notebook : Document::ViewMode::Infinite);

    // strokes
    {
      QSqlQuery q(db);
      if (q.prepare("SELECT id,color_rgba,base_width,is_shape,shape_type,shape_params FROM strokes ORDER BY id") && 
          execOrErr(q, err)) {
        
        while (q.next()) {
          Stroke s;
          s.id = q.value(0).toLongLong();
          s.color = unpackColorRgba(q.value(1).toInt());
          s.baseWidthPoints = q.value(2).toDouble();
          s.isShape = q.value(3).toInt() != 0;
          s.shapeType = q.value(4).toString();
          s.shapeParams = q.value(5).toByteArray();

          maxStrokeId = std::max(maxStrokeId, s.id);

          // points
          QSqlQuery qp(db);
          qp.prepare("SELECT x,y,pressure,t FROM stroke_points WHERE stroke_id=? ORDER BY seq");
          qp.addBindValue(s.id);
          if (execOrErr(qp, err)) {
              while (qp.next()) {
                const double x = qp.value(0).toDouble();
                const double y = qp.value(1).toDouble();
                const float pr = static_cast<float>(qp.value(2).toDouble());
                const qint64 t = qp.value(3).toLongLong();
                s.pts.push_back(StrokePoint{QPointF(x, y), pr, t});
              }
          }
          doc->insertStroke(-1, std::move(s));
        }
      }
    }

    // text boxes
    {
      QSqlQuery q(db);
      if (q.prepare("SELECT id,x,y,w,h,markdown FROM text_boxes ORDER BY id") && execOrErr(q, err)) {
          while (q.next()) {
            TextBox t;
            t.id = q.value(0).toLongLong();
            t.rectWorld = QRectF(q.value(1).toDouble(), q.value(2).toDouble(), 
                                 q.value(3).toDouble(), q.value(4).toDouble());
            t.markdown = q.value(5).toString();

            maxTextId = std::max(maxTextId, t.id);
            doc->insertTextBox(-1, std::move(t));
          }
      }
    }

    doc->setNextIds(maxStrokeId + 1, maxTextId + 1);
    db.close();
  }
  QSqlDatabase::removeDatabase(conn);
  return true;
}