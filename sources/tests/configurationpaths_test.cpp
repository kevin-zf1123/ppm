#include "configurationpaths.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class ConfigurationPathsTest : public QObject {
  Q_OBJECT

private slots:
  void configurationDirectory() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString bundle = root.filePath("PPM 日本語.app/Contents/MacOS");
    QVERIFY(QDir().mkpath(bundle));
#ifdef Q_OS_MACOS
    QCOMPARE(ConfigurationPaths::configurationDirectory(bundle), root.path());
#else
    QCOMPARE(ConfigurationPaths::configurationDirectory(bundle), bundle);
#endif
    const QString plain = root.filePath("bin");
    QVERIFY(QDir().mkpath(plain));
    QCOMPARE(ConfigurationPaths::configurationDirectory(plain), plain);

    const QString nonBundle = root.filePath("plain/Contents/MacOS");
    QVERIFY(QDir().mkpath(nonBundle));
    QCOMPARE(ConfigurationPaths::configurationDirectory(nonBundle), nonBundle);
  }

  void stuffFolder_data() {
    QTest::addColumn<bool>("bundle");
    QTest::addColumn<QString>("path");
    for (bool bundle : {false, true}) {
      const QByteArray prefix = bundle ? "bundle-" : "plain-";
      QTest::newRow(prefix + "missing") << bundle << QString();
      QTest::newRow(prefix + "empty") << bundle << QString("");
      QTest::newRow(prefix + "relative") << bundle << QString("../制作 data");
      QTest::newRow(prefix + "absolute") << bundle << QDir::tempPath();
    }
  }

  void stuffFolder() {
    QFETCH(bool, bundle);
    QFETCH(QString, path);
    QTemporaryDir root;
    QTemporaryDir unrelatedWorkingDirectory;
    QVERIFY(root.isValid());
    QVERIFY(unrelatedWorkingDirectory.isValid());
    const QString executableDir =
        root.filePath(bundle ? "PPM.app/Contents/MacOS" : "bin");
    QVERIFY(QDir().mkpath(executableDir));
    QString configDir = executableDir;
#ifdef Q_OS_MACOS
    if (bundle) configDir = root.path();
#endif
    if (!path.isNull()) {
      QSettings settings(configDir + "/conf.ini", QSettings::IniFormat);
      settings.setValue("StuffFolderPath", path);
      settings.sync();
      QCOMPARE(settings.status(), QSettings::NoError);
    }
    const QString oldWorkingDirectory = QDir::currentPath();
    QVERIFY(QDir::setCurrent(unrelatedWorkingDirectory.path()));
    const QString actual = ConfigurationPaths::stuffFolderPath(executableDir);
    QVERIFY(QDir::setCurrent(oldWorkingDirectory));
    const QString expected =
        path.isEmpty() ? QString() : QDir(configDir).absoluteFilePath(path);
    QCOMPARE(actual, QDir::cleanPath(expected));
  }

  void userSettingsLocation() {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName("PPM");
#ifdef Q_OS_MACOS
    QCOMPARE(
        ConfigurationPaths::userSettingsFilePath(),
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
            "/usersettings.ini");
    QVERIFY(ConfigurationPaths::userSettingsFilePath().endsWith(
        "/PPM/usersettings.ini"));
#else
    QCOMPARE(ConfigurationPaths::userSettingsFilePath(),
             QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
                 "/usersettings.ini");
#endif
  }
};

QTEST_GUILESS_MAIN(ConfigurationPathsTest)
#include "configurationpaths_test.moc"
