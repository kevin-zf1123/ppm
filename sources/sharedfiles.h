#pragma once

#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QScopedValueRollback>

namespace SharedFiles {
// A refresh shares one directory index across its parallel scan workers.
// Mutating file operations always look at the current directory contents.
class RefreshCache {
  QHash<QString, QHash<QString, QString>> directories;
  QMutex mutex;
  static thread_local RefreshCache* current;
  friend QString actualPath(const QString& path);

public:
  QScopedValueRollback<RefreshCache*> use() {
    return QScopedValueRollback<RefreshCache*>(current, this);
  }
};

QString comparisonName(const QString& name);
// Return the on-disk spelling. A null result means that more than one
// directory entry has the requested canonical Unicode spelling.
QString actualPath(const QString& path);
bool sameEntry(const QString& a, const QString& b);
bool insideSource(const QString& source, const QString& destination);
bool isMetadata(const QFileInfo& entry);
enum class Cleanup { Removed, Kept, Failed };
enum class EmptyFolderPolicy { CopyUndo, IgnoreHiddenAndSystem };
Cleanup removeEmptyFolder(const QString& path, EmptyFolderPolicy policy =
                                                   EmptyFolderPolicy::CopyUndo);
}  // namespace SharedFiles
