#include "platform.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class PlatformTest : public QObject {
  Q_OBJECT
private slots:
  void configurationPaths() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString root = temp.path();
    QVERIFY(QDir().mkpath(root + "/PPM.app/Contents/MacOS"));
#ifdef Q_OS_MACOS
    QCOMPARE(Platform::configurationDirectory(root + "/PPM.app/Contents/MacOS"),
             root);
    QCOMPARE(
        Platform::userSettingsFilePath(),
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
            "/usersettings.ini");
#endif
    QCOMPARE(Platform::configurationDirectory(root), root);
  }

  void destinationContainment() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString source = temp.filePath("Source");
    QVERIFY(QDir().mkpath(source));
    QVERIFY(Platform::isSameOrInsideDirectory(source, source));
    QVERIFY(Platform::isSameOrInsideDirectory(source, source + "/new/child"));
    QVERIFY(!Platform::isSameOrInsideDirectory(source, source + "-other"));
#ifdef Q_OS_MACOS
    const QString alias = temp.filePath("alias");
    QVERIFY(QFile::link(source, alias));
    QVERIFY(Platform::isSameOrInsideDirectory(source, alias + "/new/child"));
    QVERIFY(QDir().mkpath(source + "/nested"));
    const QString nestedAlias = temp.filePath("nested-alias");
    QVERIFY(QFile::link(source + "/nested", nestedAlias));
    QVERIFY(Platform::isSameOrInsideDirectory(source, nestedAlias + "/new"));
    const QString lower = temp.filePath("source");
    if (QFileInfo::exists(lower))
      QVERIFY(Platform::isSameOrInsideDirectory(source, lower + "/new"));
    else {
      QVERIFY(QDir().mkpath(lower));
      QVERIFY(!Platform::isSameOrInsideDirectory(source, lower));
    }
#endif
  }

  void launchExecutable() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString output   = temp.filePath("arguments.json");
    const QString document = temp.filePath("カット ' [01].tnz");
    QVERIFY(Platform::openFile(QCoreApplication::applicationFilePath(),
                               document,
                               {"--capture", output, "option with spaces"}));
    QTRY_VERIFY(QFileInfo::exists(output));
    QFile result(output);
    QVERIFY(result.open(QIODevice::ReadOnly));
    const QJsonArray args = QJsonDocument::fromJson(result.readAll()).array();
    QCOMPARE(args, QJsonArray({"option with spaces",
                               QDir::toNativeSeparators(document)}));
    QVERIFY(!Platform::openFile(temp.filePath("missing-executable"), document));
  }

#ifdef Q_OS_MACOS
  void launchApplicationBundle() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString bundle = temp.filePath("Test Application.app");
    QVERIFY(QDir().mkpath(bundle + "/Contents/MacOS"));
    QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(),
                        bundle + "/Contents/MacOS/capture"));
    QFile plist(bundle + "/Contents/Info.plist");
    QVERIFY(plist.open(QIODevice::WriteOnly));
    plist.write(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<plist version=\"1.0\"><dict>"
        "<key>CFBundleExecutable</key><string>capture</string>"
        "<key>CFBundleIdentifier</key><string>org.opentoonz.ppm.test</string>"
        "<key>CFBundlePackageType</key><string>APPL</string>"
        "</dict></plist>");
    plist.close();
    const QString document = temp.filePath("原画 ' [01].tnz");
    QVERIFY(Platform::openFile(bundle, document, {},
                               Platform::FileOpenMode::CommandLine));
    const QString output = document + ".json";
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(output), 10000);
    QFile result(output);
    QVERIFY(result.open(QIODevice::ReadOnly));
    QCOMPARE(QJsonDocument::fromJson(result.readAll()).array(),
             QJsonArray({document}));
    result.close();
    QVERIFY(QFile::remove(output));
    QVERIFY(Platform::openFile(bundle, document,
                               {"--capture", output, "option with spaces"}));
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(output), 10000);
    QVERIFY(result.open(QIODevice::ReadOnly));
    QCOMPARE(QJsonDocument::fromJson(result.readAll()).array(),
             QJsonArray({"option with spaces", document}));
  }

  void invalidApplicationBundle() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString bundle = temp.filePath("Invalid App.app");
    QVERIFY(QDir().mkpath(bundle));
    QVERIFY(!Platform::openFile(bundle, temp.filePath("scene.tnz")));
  }

  void zipContentsAndFailures() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString source = temp.filePath("原画 ' [01]");
    QVERIFY(QDir().mkpath(source + "/nested"));
    const QString file = source + "/nested/カット ' [01].txt";
    QFile input(file);
    QVERIFY(input.open(QIODevice::WriteOnly));
    QCOMPARE(input.write("scene data"), qint64(10));
    input.close();
    const QString zip = temp.filePath("出力 ' [01].zip");
    QVERIFY(Platform::zipFolder(source, zip));

    const QString extracted = temp.filePath("extracted");
    QProcess unzip;
    unzip.start("/usr/bin/ditto", {"-x", "-k", zip, extracted});
    QVERIFY(unzip.waitForFinished());
    QCOMPARE(unzip.exitCode(), 0);
    QFile result(extracted + "/nested/カット ' [01].txt");
    QVERIFY(result.open(QIODevice::ReadOnly));
    QCOMPARE(result.readAll(), QByteArray("scene data"));
    QVERIFY(!QFileInfo::exists(extracted + "/__MACOSX"));

    // An overwrite replaces the archive instead of retaining obsolete entries.
    QVERIFY(QFile::remove(file));
    QVERIFY(Platform::zipFolder(source, zip));
    QProcess listing;
    listing.start("/usr/bin/unzip", {"-Z1", zip});
    QVERIFY(listing.waitForFinished());
    QCOMPARE(listing.exitCode(), 0);
    QVERIFY(!listing.readAllStandardOutput().contains(".txt"));

    QFile archive(zip);
    QVERIFY(archive.open(QIODevice::ReadOnly));
    const QByteArray previous = archive.readAll();
    archive.close();
    QVERIFY(!Platform::zipFolder(temp.filePath("missing"), zip));
    QVERIFY(archive.open(QIODevice::ReadOnly));
    QCOMPARE(archive.readAll(), previous);
    QVERIFY(!Platform::zipFolder(source, source + "/self.zip"));
    QVERIFY(!Platform::zipFolder(source, temp.filePath("absent/output.zip")));
    QVERIFY(!Platform::zipFolder(source, temp.path()));
    const QString alias = temp.filePath("archive-alias.zip");
    QVERIFY(QFile::link(zip, alias));
    QVERIFY(!Platform::zipFolder(source, alias));
    const QString dangling = temp.filePath("dangling.zip");
    QVERIFY(QFile::link(source + "/not-created.zip", dangling));
    QVERIFY(!Platform::zipFolder(source, dangling));
    QVERIFY(!QFileInfo::exists(source + "/not-created.zip"));
  }
#endif
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  app.setApplicationName("PPM-platform-tests");
  QStandardPaths::setTestModeEnabled(true);
  const QStringList args = app.arguments();
  if (args.size() >= 4 && args[1] == "--capture") {
    QFile output(args[2]);
    if (!output.open(QIODevice::WriteOnly)) return 1;
    QJsonArray captured;
    for (const QString& arg : args.mid(3)) captured.append(arg);
    return output.write(QJsonDocument(captured).toJson()) < 0 ? 1 : 0;
  }
  if (args.size() == 2 && args[1].endsWith(".tnz")) {
    QFile output(args[1] + ".json");
    if (!output.open(QIODevice::WriteOnly)) return 1;
    return output.write(QJsonDocument(QJsonArray({args[1]})).toJson()) < 0 ? 1
                                                                           : 0;
  }
  PlatformTest test;
  return QTest::qExec(&test, argc, argv);
}

#include "platform_test.moc"
