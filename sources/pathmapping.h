#pragma once

#include <QList>
#include <QSettings>
#include <QStringList>

struct PathMapping {
  QString stored;
  QString local;
};

class PathMappings {
public:
  QList<PathMapping> rows;

  static bool isAbsolute(const QString& path);
  QString toLocal(const QString& stored, bool* matched = nullptr) const;
  QString filesystemPath(const QString& stored) const;
  // One option can be saved directly. Several require a choice; none means
  // the editor must confirm saving an unmapped absolute path.
  QStringList storageOptions(const QString& original,
                             const QString& input) const;
  bool accepts(const PathMapping& row) const;
  static PathMappings read(QSettings& ini);
  void write(QSettings& ini) const;
};
