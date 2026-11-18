#include "sharedfiles.h"
#include "usersettings.h"

#include <QDir>
#include <filesystem>
#include <algorithm>

namespace SharedFiles {
namespace {
constexpr auto allEntries =
    QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System;

QHash<QString, QString> directoryNames(const QDir& dir) {
  QHash<QString, QString> names;
  for (const QString& entry : dir.entryList(allEntries, QDir::Unsorted)) {
    const QString key = entry.normalized(QString::NormalizationForm_C);
    if (names.contains(key))
      names[key].clear();
    else
      names.insert(key, entry);
  }
  return names;
}
}  // namespace

thread_local RefreshCache* RefreshCache::current = nullptr;

QString comparisonName(const QString& name) {
  return UserSettings::instance()->mixedEnvironment()
             ? name.normalized(QString::NormalizationForm_C)
             : name;
}

QString actualPath(const QString& path) {
  if (!UserSettings::instance()->mixedEnvironment()) return path;
  // Only components with a canonical decomposition can have another
  // composed/decomposed spelling. Avoid directory reads for ASCII paths.
  if (path.isEmpty() || std::all_of(path.cbegin(), path.cend(),
                                    [](QChar c) { return c.unicode() < 128; }))
    return path;
  QStringList parts = QDir::fromNativeSeparators(path).split('/');
  for (int i = 0; i < parts.size(); ++i) {
    const QString wanted = parts[i].normalized(QString::NormalizationForm_C);
    if (wanted == parts[i].normalized(QString::NormalizationForm_D) &&
        wanted == parts[i])
      continue;
    QString parent = parts.mid(0, i).join('/');
#ifdef Q_OS_WIN
    if (parent.size() == 2 && parent.endsWith(':')) parent += '/';
#endif
    QDir dir(i && parent.isEmpty() ? "/" : parent);
    QString actual;
    if (auto* cache = RefreshCache::current) {
      QMutexLocker lock(&cache->mutex);
      auto index = cache->directories.find(dir.path());
      if (index == cache->directories.end())
        index = cache->directories.insert(dir.path(), directoryNames(dir));
      actual = index.value().value(wanted, parts[i]);
    } else {
      actual = directoryNames(dir).value(wanted, parts[i]);
    }
    if (actual.isEmpty()) return {};
    parts[i] = actual;
  }
  return parts.join('/');
}

bool sameEntry(const QString& a, const QString& b) {
  if (a.isEmpty() || b.isEmpty()) return false;
  std::error_code error;
#ifdef Q_OS_MACOS
  // Qt passes decomposed UTF-8 to the native filesystem on macOS, including
  // SMB.
  return std::filesystem::equivalent(QFile::encodeName(a).constData(),
                                     QFile::encodeName(b).constData(), error);
#else
  return std::filesystem::equivalent(QFileInfo(a).filesystemFilePath(),
                                     QFileInfo(b).filesystemFilePath(), error);
#endif
}

bool insideSource(const QString& source, const QString& destination) {
  QString ancestor = QFileInfo(destination).absoluteFilePath();
  while (!ancestor.isEmpty()) {
    const QString canonical = QFileInfo(ancestor).canonicalFilePath();
    if (!canonical.isEmpty()) {
      ancestor = canonical;
      if (sameEntry(source, ancestor)) return true;
    }
    const QString parent = QFileInfo(ancestor).absolutePath();
    if (parent == ancestor) break;
    ancestor = parent;
  }
  return false;
}

bool isMetadata(const QFileInfo& entry) {
  return UserSettings::instance()->mixedEnvironment() &&
         (entry.fileName() == ".DS_Store" ||
          entry.fileName().startsWith("._")) &&
         !entry.isSymLink() && entry.isFile();
}

Cleanup removeEmptyFolder(const QString& path, EmptyFolderPolicy policy) {
  if (path.isEmpty() || QFileInfo(path).isSymLink()) return Cleanup::Kept;
  QDir dir(path);
  if (!dir.exists()) return Cleanup::Failed;
  const auto entries      = dir.entryInfoList(allEntries, QDir::Unsorted);
  const bool ignoreHidden = policy == EmptyFolderPolicy::IgnoreHiddenAndSystem;
  // Take the deletion list first. A new visible file must make the final rmdir
  // fail.
  const auto visible =
      ignoreHidden ? dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot,
                                       QDir::Unsorted)
                   : entries;
  for (const auto& entry : visible)
    if (!isMetadata(entry)) return Cleanup::Kept;
  for (const auto& entry : entries) {
    // An already hidden directory is part of the ignored contents. Never
    // recursively delete the visible parent after checking its contents.
    const bool removed = entry.isDir() && !entry.isSymLink()
                             ? QDir(entry.filePath()).removeRecursively()
                             : QFile::remove(entry.filePath());
    if (!removed) return Cleanup::Failed;
  }
  return QDir().rmdir(path) ? Cleanup::Removed : Cleanup::Failed;
}
}  // namespace SharedFiles
