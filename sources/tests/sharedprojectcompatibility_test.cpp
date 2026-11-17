#include "actionconfirmationdialog.h"
#include "configurationpaths.h"
#include "mywindow.h"
#include "projectsettings_common.h"
#include "projectsettingsdialog.h"
#include "sharedfiles.h"
#include "usersettings.h"

#include <QFile>
#include <QProcess>
#include <memory>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <QJsonDocument>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QInputDialog>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

class SharedProjectCompatibilityTest : public QObject {
  Q_OBJECT
  // An explicit test root allows the same actions to run on a mounted share.
  QTemporaryDir root{qEnvironmentVariable("PPM_TEST_VOLUME", QDir::tempPath()) +
                     "/ppm-shared-XXXXXX"};
  QTemporaryDir settings;
  QString confPath;
  bool ownsConfig   = false;
  bool ownsUserFile = false;
  QString stored    = QStringLiteral(R"(\\Studio\Anime)");
  PathMappings defaults;

  static bool write(const QString& path, const QByteArray& data) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
  }

private slots:
  void initTestCase() {
    QVERIFY(root.isValid());
    QVERIFY(settings.isValid());
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QFileInfo(settings.path()).fileName());
    QString config = ConfigurationPaths::configurationDirectory(
        QCoreApplication::applicationDirPath());
    QVERIFY(QDir().mkpath(config));
    confPath = config + "/conf.ini";
    QVERIFY2(!QFileInfo::exists(confPath),
             "Expected isolated test executable/configuration directory");
    ownsConfig = true;
    QVERIFY(!QFileInfo::exists(ConfigurationPaths::userSettingsFilePath()));
    ownsUserFile = true;
    QSettings(confPath, QSettings::IniFormat)
        .setValue("StuffFolderPath", root.path());
    QVERIFY(QDir().mkpath(root.filePath("projects")));
    defaults.rows = {{stored, root.path()},
                     {root.path(), root.filePath("double-mapped")}};
#ifdef Q_OS_WIN
    QString name = "defaultusersettings_windows.ini";
#else
    QString name = "defaultusersettings_macos.ini";
#endif
    QSettings ini(root.filePath(name), QSettings::IniFormat);
    defaults.write(ini);
    ini.sync();
    // A platform mapping without an explicit opt-in must leave Windows-only
    // studios on the normal filesystem path, including after an upgrade.
    QVERIFY(write(root.filePath(QStringLiteral("ガ.lookup")), "cue"));
    QProcess plain;
    plain.start(QCoreApplication::applicationFilePath(),
                {"--mapping-settings", QCoreApplication::applicationName(),
                 "off", stored, stored, root.path()});
    QVERIFY(plain.waitForFinished());
    QCOMPARE(plain.exitStatus(), QProcess::NormalExit);
    QCOMPARE(plain.exitCode(), 0);
    ini.setValue("UserSettings/MixedEnvironment", true);
    ini.sync();
    QCOMPARE(UserSettings::instance()->pathMappings().toLocal(stored),
             root.path());
  }

  void cleanupTestCase() {
    if (ownsConfig) QFile::remove(confPath);
    if (ownsUserFile) QFile::remove(ConfigurationPaths::userSettingsFilePath());
  }

  void matchPrefixes() {
    PathMappings m;
    QCOMPARE(m.toLocal("Z:/anime"), "Z:/anime");
    m.rows = {{"Z:/anime", "/Volumes/Anime"},
              {"z:/anime/cuts", "/Volumes/Anime/cuts"},
              {"//NAS/anime", "/Volumes/Anime"}};
    QCOMPARE(m.toLocal(R"(z:\ANIME\CUTS\A.clip)"),
             "/Volumes/Anime/cuts/A.clip");
    QCOMPARE(m.toLocal("Z:/anime2/cuts"), "Z:/anime2/cuts");
    QCOMPARE(m.toLocal("[ROOT]/cuts"), "[ROOT]/cuts");
    QCOMPARE(m.toLocal("relative/cuts"), "relative/cuts");
    QCOMPARE(m.storedCandidates("/Volumes/Anime/cuts/A.clip").size(), 3);
    m.rows.append(PathMapping{"//NAS/anime/other", "/Volumes/B"});
    QVERIFY(!m.storedCandidates("/Volumes/Anime/other/file")
                 .contains("//NAS/anime/other/file"));
    QCOMPARE(m.toLocal(R"(\\nas\ANIME\01)"), "/Volumes/Anime/01");
  }

  void overrideDefaults() {
    auto* user = UserSettings::instance();
    QVERIFY(user->inheritsMappings());
    user->setPathMappings({}, false);
    QVERIFY(!user->inheritsMappings());
    QSettings ini(ConfigurationPaths::userSettingsFilePath(),
                  QSettings::IniFormat);
    QVERIFY(ini.contains("PathMappings/size"));
    QCOMPARE(PathMappings::read(ini).rows.size(), 0);
    PathMappings personal;
    personal.rows = {{stored, root.filePath("personal")}};
    user->setPathMappings(personal, false);
    user->setPathMappings({}, true);
    QCOMPARE(PathMappings::read(ini).toLocal(stored),
             root.filePath("personal"));
    QVERIFY(user->inheritsMappings());
    QCOMPARE(user->personalMappings().toLocal(stored),
             root.filePath("personal"));
    // Each child starts PPM with the settings just written to disk. Switching
    // back to personal mappings must restore the saved prefix after a restart.
    auto restart = [&](const QString& mode, const QString& local) {
      QProcess child;
      child.start(QCoreApplication::applicationFilePath(),
                  {"--mapping-settings", QCoreApplication::applicationName(),
                   mode, stored, local, root.filePath("personal")});
      QVERIFY(child.waitForFinished());
      QCOMPARE(child.exitStatus(), QProcess::NormalExit);
      QCOMPARE(child.exitCode(), 0);
    };
    restart("on", root.path());
    user->setPathMappings(user->personalMappings(), false);
    restart("on", root.filePath("personal"));
    user->setMixedEnvironment(false);
    restart("off", stored);
    user->setMixedEnvironment(true);
    user->setPathMappings({}, false);
    restart("on", stored);
    user->setPathMappings({}, true);
  }

  void projectEditorAndScan() {
    auto* wm = WorkflowManager::instance();
    wm->setProjectRootPath(stored);
    wm->setInfoFolderPath(stored + "/info");
    wm->setLogRootPath(stored + "/logs");
    wm->setSceneThumbnailPath("[ROOT]/[CUTNUMBER].png");
    wm->setUseSequence(false);
    wm->setProjectTitle("Title");
    wm->setPathMacros({{"[HIDDEN]", stored + "/private", {}, true}});
    FlowGroupInfo group;
    group.idName = "Layout";
    wm->setFlowGroupInfos({{0, group}});
    Workflow wf{0, 0, "Layout", Qt::white};
    wf.protocols = {{Digital, "[ROOT]/[CUTNUMBER].clip", {}}};
    wm->setWorkflows({wf});
    QVERIFY(wm->saveProject("shared"));
    QVERIFY(wm->loadProject("shared"));
    refreshTmpSettingsFromProject();
    BasicsPage basics;
    basics.refresh();
    auto folders   = basics.findChildren<FolderPathField*>();
    auto rootField = std::find_if(
        folders.begin(), folders.end(),
        [&](FolderPathField* f) { return f->text() == root.path(); });
    QVERIFY(rootField != folders.end());
    QVERIFY(
        (*rootField)->toolTip().contains(QDir::fromNativeSeparators(stored)));
    auto edits = basics.findChildren<QLineEdit*>();
    auto title = std::find_if(edits.begin(), edits.end(), [](QLineEdit* edit) {
      return edit->text() == "Title";
    });
    QVERIFY(title != edits.end());
    (*title)->setText("Edited title");
    QVERIFY(QMetaObject::invokeMethod(&basics, "onFieldEdited"));
    QVERIFY(saveTmpSettingsToProject(nullptr, "shared"));
    QVERIFY(wm->loadProject("shared"));
    QCOMPARE(wm->projectTitle(), "Edited title");
    QCOMPARE(wm->projectRootPathSetting(), QDir::fromNativeSeparators(stored));
    QCOMPARE(wm->logRootPathSetting(),
             QDir::fromNativeSeparators(stored + "/logs"));
    QCOMPARE(wm->getWorkflow(0).protocols.first().conditionPath,
             "[ROOT]/[CUTNUMBER].clip");
    QCOMPARE(wm->decodePath("[ROOT]/drawing", {}, {}),
             root.filePath("drawing"));
    QCOMPARE(wm->decodeObfuscatedPathMacros("[HIDDEN]/x", {}, {}),
             root.filePath("private/x"));
    QCOMPARE(wm->hideObfuscatedPaths(root.filePath("private/x"), {}, {}),
             "[HIDDEN]/x");
    QVERIFY(write(root.filePath("0001.clip"), "drawing"));
    wm->registerCutNames({"0001"}, 0);
    MyTable table;
    table.simpleSearch(0);
    QVERIFY(wm->isInProgress("0001", 0));
    // This local path is also the stored prefix of a second rule. The
    // editor must offer both interpretations, including cancel.
    QString input = root.filePath("ambiguous");
    (*rootField)->setText(input);
    QTimer::singleShot(0, [] {
      auto* chooser =
          qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
      QVERIFY(chooser);
      chooser->reject();
    });
    QVERIFY(QMetaObject::invokeMethod(&basics, "onFieldEdited"));
    QCOMPARE(tmpSettings.projectRootPath, QDir::fromNativeSeparators(stored));
    (*rootField)->setText(input);
    QTimer::singleShot(0, [&] {
      auto* chooser =
          qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
      QVERIFY(chooser);
      const QString chosen = "//Studio/Anime/ambiguous";
      QVERIFY(chooser->comboBoxItems().contains(chosen));
      chooser->setTextValue(chosen);
      chooser->accept();
    });
    QVERIFY(QMetaObject::invokeMethod(&basics, "onFieldEdited"));
    QCOMPARE(tmpSettings.projectRootPath, "//Studio/Anime/ambiguous");
    (*rootField)->setText("//Other/Unmapped");
    QTimer::singleShot(0, [] {
      auto* warning =
          qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
      QVERIFY(warning);
      warning->button(QMessageBox::Save)->click();
    });
    QVERIFY(QMetaObject::invokeMethod(&basics, "onFieldEdited"));
    QCOMPARE(tmpSettings.projectRootPath, "//Other/Unmapped");
    QVERIFY((*rootField)->toolTip().contains("//Other/Unmapped"));
    // A hand-entered stored path must round-trip through the local display.
    (*rootField)->setText("//Studio/Anime/New");
    QVERIFY(QMetaObject::invokeMethod(&basics, "onFieldEdited"));
    QCOMPARE((*rootField)->text(), root.filePath("New"));
    QVERIFY(saveTmpSettingsToProject(nullptr, "shared"));
    QVERIFY(wm->loadProject("shared"));
    QCOMPARE(wm->projectRootPathSetting(), "//Studio/Anime/New");
    QCOMPARE(wm->infoFolderPathSetting(),
             QDir::fromNativeSeparators(stored + "/info"));
    QCOMPARE(wm->sceneThumbnailPath(), "[ROOT]/[CUTNUMBER].png");
  }

  void rewindOnMappedShare() {
    auto* wm = WorkflowManager::instance();
    wm->setProjectRootPath(stored);
    wm->setInfoFolderPath(stored + "/info");
    wm->setLogRootPath(stored + "/logs");
    FlowGroupInfo group;
    group.idName            = "Layout";
    group.takeNotationStyle = FlowGroupInfo::TakeNotation_Rewind;
    wm->setFlowGroupInfos({{0, group}});
    Workflow wf{0, 0, "Layout", Qt::white};
    wf.protocols = {{Digital, "[ROOT]/rewind/[CUTNUMBER][R].clip", {}}};
    wm->setWorkflows({wf});
    QString cue = root.filePath("rewind/0002.clip");
    QVERIFY(QDir().mkpath(root.filePath("rewind")));
    QVERIFY(write(cue, "drawing"));
    RewindConfirmationDialog dialog({0, 0, {cue}, {}, "0002"}, {0, 0}, false);
    for (auto* button : dialog.findChildren<QPushButton*>())
      if (button->text() == "OK") {
        button->click();
        break;
      }
    QVERIFY(!dialog.isCanceled());
    QVERIFY(QFileInfo::exists(root.filePath("rewind/old/0002.clip")));
    QVERIFY(QFileInfo::exists(root.filePath("rewind/0002_r.clip.txt")));
    QVERIFY(!QFileInfo::exists(root.filePath("double-mapped")));
  }

  void recursiveCopyAndUndo() {
    const QString source = root.filePath("Source");
    const QString dest   = root.filePath("Destination");
    QVERIFY(QDir().mkpath(source + "/empty"));
    const QString kana =
        QStringLiteral("ガ").normalized(QString::NormalizationForm_D);
    QVERIFY(write(source + "/" + kana, "drawing"));
    QVERIFY(write(source + "/.notes", "user file"));
    QVERIFY(write(source + "/empty/.DS_Store", "metadata"));
    QVERIFY(write(source + "/._drawing", "metadata"));
    const ActionStep step{Step_Copy, {stored + "/Destination"}};
    StepExecutor_Copy copy(step, source, {}, {}, false);
    QString log;
    QVariant record;
    QVERIFY2(copy.execute(log, record), qPrintable(log));
    const QString actual = SharedFiles::actualPath(dest + "/ガ");
    QVERIFY(QFileInfo(actual).isFile());
    QVERIFY(QFileInfo(dest + "/.notes").isFile());
    QVERIFY(!QFileInfo::exists(dest + "/empty/.DS_Store"));
    QVERIFY(!QFileInfo::exists(dest + "/._drawing"));
    QVERIFY(write(dest + "/empty/.DS_Store", "metadata"));
    QVERIFY(write(dest + "/keep.clip", "later"));
    StepExecutor_Copy undo(step, source, {}, {}, true);
    undo.setCheckSum(record);
    QVERIFY2(undo.execute(log, record), qPrintable(log));
    QVERIFY(QFileInfo(dest + "/keep.clip").isFile());
    QVERIFY(!QFileInfo::exists(dest + "/empty"));
    QVERIFY(!QFileInfo::exists(actual));
    QVERIFY(QDir().mkpath(root.filePath("metadata-only")));
    QVERIFY(write(root.filePath("metadata-only/._drawing"), "metadata"));
    QCOMPARE(SharedFiles::removeEmptyFolder(root.filePath("metadata-only")),
             SharedFiles::Cleanup::Removed);
    QVERIFY(QFileInfo::exists(source + "/" + kana));
    QVERIFY(SharedFiles::insideSource(source, source + "/missing/child"));
    const QString alias = root.filePath("sOuRcE");
    QCOMPARE(SharedFiles::insideSource(source, alias + "/new"),
             QFileInfo(alias).exists());
  }

  void unavailableForeignRoot() {
    auto* wm = WorkflowManager::instance();
#ifdef Q_OS_WIN
    const QString foreign = "/Volumes/Unmapped";
#else
    const QString foreign = "Z:/Unmapped";
#endif
    wm->setProjectRootPath(foreign);
    wm->setLogRootPath(foreign);
    wm->setInfoFolderPath(foreign);
    QCOMPARE(wm->projectRootPathSetting(), foreign);
    QVERIFY(wm->projectRootPath().isEmpty());
    QVERIFY(wm->decodePath("[ROOT]/file", {}, {}).isEmpty());
    QVERIFY(wm->googleCueFilePath("log").isEmpty());
    QString settingsFile, lock;
    wm->getProgressSettingsFilePath(0, settingsFile, lock);
    QVERIFY(settingsFile.isEmpty());
    QVERIFY(lock.isEmpty());
  }

  // Move, Ask Delete and undoing Create Empty Folder all treat a folder
  // containing only hidden or system files as empty.
  void hiddenFolderCleanup_data() {
    QTest::addColumn<QString>("action");
    QTest::newRow("move source") << QString("move");
    QTest::newRow("ask delete") << QString("delete");
    QTest::newRow("undo create empty folder") << QString("undo");
  }

  void hiddenFolderCleanup() {
    QFETCH(QString, action);
    const QString group = "/cleanup-" + action;
    const QString dir   = root.path() + group + "/scene";
    QVERIFY(QDir().mkpath(dir));
#ifdef Q_OS_WIN
    const QString hidden = dir + "/desktop.ini";
    QVERIFY(write(hidden, "shell metadata"));
    QVERIFY(SetFileAttributesW(reinterpret_cast<LPCWSTR>(hidden.utf16()),
                               FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM));
#else
    QVERIFY(write(dir + "/.hidden", "hidden"));
#endif
    QString log;
    QVariant record;
    if (action == "move") {
      QVERIFY(write(dir + "/drawing.clip", "drawing"));
      StepExecutor_CopyFolderContents move(
          0,
          {Step_CopyFolderContents,
           {"[ALL_WITHOUT_UI][MOVE]", stored + group, stored + "/moved"}},
          dir + "/drawing.clip", {}, "0001", false);
      QVERIFY2(move.execute(log, record), qPrintable(log));
      QVERIFY(QFileInfo::exists(root.filePath("moved/scene/drawing.clip")));
    } else if (action == "delete") {
      StepExecutor_AskDelete remove(
          {Step_AskDelete, {true, stored + group + "/scene"}}, {}, {}, {},
          false);
      std::unique_ptr<QWidget> options(remove.createWidget());
      QVERIFY2(remove.execute(log, record), qPrintable(log));
    } else {
      // A drawing added after folder creation must survive undo. Once the
      // drawing is removed, the same undo can clean up the remaining metadata.
      const QString visible = dir + "/keep.clip";
      QVERIFY(write(visible, "drawing"));
      StepExecutor_CreateEmptyFolder undo(
          {Step_CreateEmptyFolder, {stored + group + "/scene"}}, {}, {}, true);
      undo.setCheckSum(true);
      QVERIFY(undo.execute(log, record));
      QVERIFY(QFileInfo::exists(visible));
      QVERIFY(QFile::remove(visible));
      QVERIFY2(undo.execute(log, record), qPrintable(log));
    }
    QVERIFY(!QFileInfo::exists(dir));
  }

  void refreshDirectorySnapshot() {
    // Create the composed filename between two lookups in one refresh.
    // Only the next refresh should see its on-disk spelling.
    const QString dir = root.filePath("listing");
    QVERIFY(QDir().mkdir(dir));
    const QString nfd =
        QStringLiteral("ガ.clip").normalized(QString::NormalizationForm_D);
    const QString nfc    = QStringLiteral("ガ.clip");
    const QString lookup = dir + "/" + nfd;
    {
      SharedFiles::RefreshCache snapshot;
      auto scope = snapshot.use();
      QCOMPARE(SharedFiles::actualPath(lookup), lookup);
      QVERIFY(write(dir + "/" + nfc, "drawing"));
      QCOMPARE(SharedFiles::actualPath(lookup), lookup);
    }
    SharedFiles::RefreshCache next;
    auto scope = next.use();
    QCOMPARE(SharedFiles::actualPath(lookup),
             dir + "/" + QDir(dir).entryList(QDir::Files).first());
  }

  void equivalentSpellings() {
    const QString composed   = root.filePath("ペ.clip");
    const QString decomposed = root.filePath(
        QStringLiteral("ペ.clip").normalized(QString::NormalizationForm_D));
    QVERIFY(write(decomposed, "a"));
    QVERIFY(
        SharedFiles::sameEntry(SharedFiles::actualPath(composed), decomposed));
    if (!QFileInfo::exists(composed)) {
      QVERIFY(write(composed, "b"));
      QVERIFY(SharedFiles::actualPath(composed).isEmpty());
      QVERIFY(SharedFiles::actualPath(decomposed).isEmpty());
      QVERIFY(QFileInfo::exists(composed));
      QVERIFY(QFileInfo::exists(decomposed));
    }
  }
};

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  const QStringList args = app.arguments();
  // The child reads the same test INI files as the parent. Check both the
  // selected mapping and the filename lookup it produces on this filesystem.
  if (args.size() == 7 && args[1] == "--mapping-settings") {
    QStandardPaths::setTestModeEnabled(true);
    app.setApplicationName(args[2]);
    const auto* user   = UserSettings::instance();
    const bool enabled = args[3] == "on";
    if (user->mixedEnvironment() != enabled ||
        user->pathMappings().toLocal(args[4]) != args[5])
      return 1;
    if (user->inheritsMappings() &&
        user->personalMappings().toLocal(args[4]) != args[6])
      return 2;
    const QString kana =
        QStringLiteral("ガ").normalized(QString::NormalizationForm_D);
    if (SharedFiles::comparisonName(kana) !=
        (enabled ? QStringLiteral("ガ") : kana))
      return 3;
    QDir root(user->stuffFolderPath());
    const QString input        = root.filePath(kana + ".lookup");
    const QStringList existing = root.entryList({"*.lookup"}, QDir::Files);
    if (existing.size() != 1 ||
        SharedFiles::actualPath(input) !=
            (enabled ? root.filePath(existing.first()) : input))
      return 4;
    return 0;
  }
  SharedProjectCompatibilityTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "sharedprojectcompatibility_test.moc"
