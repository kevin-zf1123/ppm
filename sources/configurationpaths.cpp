#include "configurationpaths.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace ConfigurationPaths {

QString configurationDirectory(const QString& executableDirectory) {
#ifdef Q_OS_MACOS
  QDir dir(executableDirectory);
  if (dir.dirName() == "MacOS" && dir.cdUp() && dir.dirName() == "Contents" &&
      dir.cdUp() && dir.dirName().endsWith(".app", Qt::CaseInsensitive) &&
      dir.cdUp())
    return dir.absolutePath();
#endif
  return QDir(executableDirectory).absolutePath();
}

QString stuffFolderPath(const QString& executableDirectory) {
  const QString configDir = configurationDirectory(executableDirectory);
  QSettings settings(configDir + "/conf.ini", QSettings::IniFormat);
  const QString path = settings.value("StuffFolderPath").toString();
  if (path.isEmpty()) return QString();
  QDir dir(path);
  if (dir.isRelative()) dir.setPath(configDir + "/" + dir.path());
  return dir.absolutePath();
}

QString userSettingsFilePath() {
#ifdef Q_OS_MACOS
  const auto location = QStandardPaths::AppConfigLocation;
#else
  const auto location = QStandardPaths::ConfigLocation;
#endif
  return QStandardPaths::writableLocation(location) + "/usersettings.ini";
}

}  // namespace ConfigurationPaths
