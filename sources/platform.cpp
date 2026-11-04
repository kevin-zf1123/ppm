#include "platform.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>

#ifdef Q_OS_MACOS
#include <sys/stat.h>
#endif

namespace Platform {

QString configurationDirectory(const QString& executableDirectory) {
  QDir dir(executableDirectory);
#ifdef Q_OS_MACOS
  if (dir.dirName() == "MacOS" && dir.cdUp() && dir.dirName() == "Contents" &&
      dir.cdUp() && dir.dirName().endsWith(".app", Qt::CaseInsensitive) &&
      dir.cdUp())
    return dir.absolutePath();
#endif
  return QDir(executableDirectory).absolutePath();
}

QString userSettingsFilePath() {
#ifdef Q_OS_MACOS
  const auto location = QStandardPaths::AppConfigLocation;
#else
  const auto location = QStandardPaths::ConfigLocation;
#endif
  return QStandardPaths::writableLocation(location) + "/usersettings.ini";
}

bool openFile(const QString& application, const QString& file,
              const QStringList& arguments, FileOpenMode mode) {
#ifdef Q_OS_MACOS
  const QFileInfo appInfo(application);
  if (appInfo.isDir() &&
      appInfo.suffix().compare("app", Qt::CaseInsensitive) == 0) {
    QStringList openArguments{"-a", appInfo.absoluteFilePath()};
    if (arguments.isEmpty() && mode == FileOpenMode::Document) {
      // Deliver a document-open event, including to an already running app.
      openArguments << QFileInfo(file).absoluteFilePath();
    } else {
      // Command-line options require a new process; preserve their ordering.
      openArguments << "-n" << "--args" << arguments
                    << QFileInfo(file).absoluteFilePath();
    }
    QProcess process;
    process.start("/usr/bin/open", openArguments);
    return process.waitForFinished(10000) &&
           process.exitStatus() == QProcess::NormalExit &&
           process.exitCode() == 0;
  }
#else
  Q_UNUSED(mode);
#endif
  QStringList processArguments = arguments;
  processArguments << QDir::toNativeSeparators(file);
  return QProcess::startDetached(application, processArguments);
}

bool isSameOrInsideDirectory(const QString& source,
                             const QString& destination) {
  const QString from = QDir(source).absolutePath();
  const QString to = QDir::cleanPath(QFileInfo(destination).absoluteFilePath());
#ifdef Q_OS_MACOS
  // File identity handles both case-sensitive and case-insensitive volumes,
  // symlinks, and the spelling of decomposed Unicode filenames on macOS.
  struct stat sourceStat;
  if (::stat(QFile::encodeName(from).constData(), &sourceStat) == 0) {
    for (QString path = to;;) {
      struct stat destinationStat;
      if (::stat(QFile::encodeName(path).constData(), &destinationStat) == 0 &&
          sourceStat.st_dev == destinationStat.st_dev &&
          sourceStat.st_ino == destinationStat.st_ino)
        return true;
      const QString canonical = QFileInfo(path).canonicalFilePath();
      if (!canonical.isEmpty()) path = canonical;
      const QString parent = QFileInfo(path).dir().absolutePath();
      if (path == parent) break;
      path = parent;
    }
  }
#endif
#ifdef Q_OS_WIN
  const auto cs = Qt::CaseInsensitive;
#else
  const auto cs = Qt::CaseSensitive;
#endif
  return to.compare(from, cs) == 0 ||
         to.startsWith(from.endsWith('/') ? from : from + "/", cs);
}

bool zipFolder(const QString& source, const QString& destination) {
  if (!QFileInfo(source).isDir() ||
      isSameOrInsideDirectory(source, destination))
    return false;

  QProcess process;
#ifdef Q_OS_MACOS
  // QSaveFile follows leaf symlinks, but Undo removes the link itself.
  if (QFileInfo(destination).isSymLink()) return false;
  // Stage outside the source tree. A failed compression must not destroy an
  // existing archive; publish only a complete archive with QSaveFile.
  QTemporaryDir staging(QFileInfo(destination).absolutePath() +
                        "/.ppm-zip-XXXXXX");
  if (!staging.isValid()) return false;
  const QString archive = staging.filePath("contents.zip");
  process.start("/usr/bin/ditto",
                {"-c", "-k", "--norsrc", "--noextattr",
                 QFileInfo(source).absoluteFilePath(), archive});
  if (!process.waitForFinished(-1) ||
      process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    return false;

  QFile input(archive);
  QSaveFile output(destination);
  if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
    return false;
  char buffer[64 * 1024];
  while (true) {
    const qint64 count = input.read(buffer, sizeof(buffer));
    if (count < 0) return false;
    if (count == 0) break;
    if (output.write(buffer, count) != count) return false;
  }
  return output.commit();
#elif defined(Q_OS_WIN)
  // Pass paths as data, not PowerShell source (including quotes and brackets).
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert("PPM_ZIP_SOURCE", QDir(source).absolutePath());
  environment.insert("PPM_ZIP_DESTINATION",
                     QFileInfo(destination).absoluteFilePath());
  process.setProcessEnvironment(environment);
  process.start(
      "powershell.exe",
      {"-NoProfile", "-NonInteractive", "-Command",
       "$ErrorActionPreference = 'Stop'; Compress-Archive -Force "
       "-Path ([System.Management.Automation.WildcardPattern]::Escape("
       "$env:PPM_ZIP_SOURCE) + '/*') "
       "-DestinationPath $env:PPM_ZIP_DESTINATION"});
  return process.waitForFinished(-1) &&
         process.exitStatus() == QProcess::NormalExit &&
         process.exitCode() == 0;
#else
  return false;
#endif
}

}  // namespace Platform
