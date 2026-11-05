#pragma once

#include <QString>

namespace ConfigurationPaths {

// conf.ini lives next to the app bundle on macOS, or next to the executable.
QString configurationDirectory(const QString& executableDirectory);
QString stuffFolderPath(const QString& executableDirectory);
QString userSettingsFilePath();

}  // namespace ConfigurationPaths
