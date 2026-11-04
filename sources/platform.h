#pragma once

#include <QString>
#include <QStringList>

namespace Platform {

// conf.ini lives beside the executable, or beside the .app on macOS.
QString configurationDirectory(const QString& executableDirectory);
QString userSettingsFilePath();

enum class FileOpenMode { Document, CommandLine };

bool openFile(const QString& application, const QString& file,
              const QStringList& arguments = {},
              FileOpenMode mode            = FileOpenMode::Document);

// Includes existing aliases and non-existing children of an existing directory.
bool isSameOrInsideDirectory(const QString& source, const QString& destination);

// The archive contains the contents of source, without the source folder name.
bool zipFolder(const QString& source, const QString& destination);

}  // namespace Platform
