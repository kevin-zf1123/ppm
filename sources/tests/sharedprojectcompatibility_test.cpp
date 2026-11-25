#include "actionconfirmationdialog.h"
#include "configurationpaths.h"
#include "mywindow.h"
#include "projectsettings_common.h"
#include "sharedfiles.h"
#include "usersettings.h"

#include <QFile>
#include <QProcess>
#include <memory>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <QStandardPaths>
#include <QTemporaryDir>
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

  static bool write(const QString& path, const QByteArray& data) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
  }

  // Run this executable with --mapping-settings: UserSettings captures the
  // active settings once per process, so only a fresh process observes a
  // restart.
  void checkRestart(const QString& mode, const QString& local,
                    const QString& personal) {
    QCOMPARE(QProcess::execute(
                 QCoreApplication::applicationFilePath(),
                 {"--mapping-settings", QCoreApplication::applicationName(),
                  mode, stored, local, personal}),
             0);
  }

  // A one-workflow project whose cue, info and log files live on the mapped
  // share.
  void configureLayout(const QString& condition = "[ROOT]/[CUTNUMBER].clip",
                       FlowGroupInfo::TakeNotationStyle style =
                           FlowGroupInfo::TakeNotation_None) {
    auto* wm = WorkflowManager::instance();
    wm->setProjectRootPath(stored);
    wm->setInfoFolderPath(stored + "/info");
    wm->setLogRootPath(stored + "/logs");
    wm->setUseSequence(false);
    FlowGroupInfo group;
    group.idName            = "Layout";
    group.takeNotationStyle = style;
    wm->setFlowGroupInfos({{0, group}});
    Workflow wf{0, 0, "Layout", Qt::white};
    wf.protocols = {{Digital, condition, {}}};
    wm->setWorkflows({wf});
  }

private slots:
  // Map the studio UNC prefix to a temporary project. Only the common defaults
  // file can opt the studio in; a platform file alone must leave it off.
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
    PathMappings defaults;
    defaults.rows = {{stored, root.path()},
                     {root.path(), root.filePath("double-mapped")}};
#ifdef Q_OS_WIN
    QString name = "defaultusersettings_windows.ini";
#else
    QString name = "defaultusersettings_macos.ini";
#endif
    QSettings ini(root.filePath(name), QSettings::IniFormat);
    defaults.write(ini);
    ini.setValue("UserSettings/MixedEnvironment", true);
    ini.sync();
    QVERIFY(write(root.filePath(QStringLiteral("ガ.lookup")), "cue"));
    checkRestart("off", stored, root.path());
    QSettings common(root.filePath("defaultusersettings.ini"),
                     QSettings::IniFormat);
    common.setValue("UserSettings/MixedEnvironment", true);
    common.sync();
    QCOMPARE(UserSettings::instance()->pathMappings().toLocal(stored),
             root.path());
  }

  // Remove only the configuration files created for this isolated test process.
  void cleanupTestCase() {
    if (ownsConfig) QFile::remove(confPath);
    if (ownsUserFile) QFile::remove(ConfigurationPaths::userSettingsFilePath());
  }

  // Drive-letter and UNC rules cover the same mount: match entire directory
  // prefixes, prefer the longest and discard reverse paths that cannot
  // round-trip.
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
    QCOMPARE(m.storageOptions({}, "/Volumes/Anime/cuts/A.clip").size(), 3);
    m.rows.append(PathMapping{"//NAS/anime/other", "/Volumes/B"});
    QVERIFY(!m.storageOptions({}, "/Volumes/Anime/other/file")
                 .contains("//NAS/anime/other/file"));
    QCOMPARE(m.toLocal(R"(\\nas\ANIME\01)"), "/Volumes/Anime/01");
  }

  // Switch between studio and personal mappings in the test INI files. After
  // restart, both the saved personal table and an explicit empty override
  // survive.
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
    checkRestart("on", root.path(), root.filePath("personal"));
    user->setPathMappings(user->personalMappings(), false);
    checkRestart("on", root.filePath("personal"), root.filePath("personal"));
    user->setMixedEnvironment(false);
    checkRestart("off", stored, root.filePath("personal"));
    user->setMixedEnvironment(true);
    user->setPathMappings({}, false);
    checkRestart("on", stored, root.filePath("personal"));
    user->setPathMappings({}, true);
  }

  // Issue #3: the original spelling survives a title-only edit; a changed
  // field can have one, several or no valid stored forms.
  void editedPathOptions() {
    const QString original = R"(\\Studio\Anime\Old)";
    PathMappings mappings;
    QCOMPARE(mappings.storageOptions(original, "/unmapped"),
             QStringList{"/unmapped"});
    mappings.rows = {{"//Studio/Anime", "/mount/art"}};
    QCOMPARE(mappings.storageOptions(original, "/mount/art/Old"),
             QStringList{original});
    QCOMPARE(mappings.storageOptions(original, "/mount/art/New"),
             QStringList{"//Studio/Anime/New"});
    QCOMPARE(mappings.storageOptions(original, "//Studio/Anime/New"),
             QStringList{"//Studio/Anime/New"});
    QCOMPARE(mappings.storageOptions(original, "[ROOT]/New"),
             QStringList{"[ROOT]/New"});
    QCOMPARE(mappings.storageOptions(original, ""), QStringList{""});
    QVERIFY(mappings.storageOptions(original, "/unmapped").isEmpty());

    // The local mount is also a stored prefix for a different location.
    mappings.rows.append(PathMapping{"/mount/art", "/elsewhere"});
    QCOMPARE(mappings.storageOptions(original, "/mount/art/New"),
             (QStringList{"/mount/art/New", "//Studio/Anime/New"}));
  }

  // Change only the title of a UNC project, reload it and scan a cue under the
  // mapped root. The stored paths and macros must survive the save.
  void projectRoundTripAndScan() {
    auto* wm = WorkflowManager::instance();
    configureLayout();
    wm->setSceneThumbnailPath("[ROOT]/[CUTNUMBER].png");
    wm->setProjectTitle("Title");
    wm->setPathMacros({{"[HIDDEN]", stored + "/private", {}, true}});
    QVERIFY(wm->saveProject("shared"));
    QVERIFY(wm->loadProject("shared"));
    refreshTmpSettingsFromProject();
    tmpSettings.projectTitle = "Edited title";
    QVERIFY(saveTmpSettingsToProject(nullptr, "shared"));
    QVERIFY(wm->loadProject("shared"));
    QCOMPARE(wm->projectTitle(), "Edited title");
    QCOMPARE(wm->projectRootPathSetting(), QDir::fromNativeSeparators(stored));
    QCOMPARE(wm->logRootPathSetting(),
             QDir::fromNativeSeparators(stored + "/logs"));
    QCOMPARE(wm->infoFolderPathSetting(),
             QDir::fromNativeSeparators(stored + "/info"));
    QCOMPARE(wm->sceneThumbnailPath(), "[ROOT]/[CUTNUMBER].png");
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
  }

  // Rewind builds its executors from the selected workflow and its file picker
  // supplies the files to move. Confirm the assembled dialog to check that its
  // local backup and cue paths are not mapped a second time.
  void rewindOnMappedShare() {
    configureLayout("[ROOT]/rewind/[CUTNUMBER][R].clip",
                    FlowGroupInfo::TakeNotation_Rewind);
    QString cue = root.filePath("rewind/0002.clip");
    QVERIFY(QDir().mkpath(root.filePath("rewind")));
    QVERIFY(write(cue, "drawing"));
    RewindConfirmationDialog dialog({0, 0, {cue}, {}, "0002"}, {0, 0}, false);
    dialog.show();
    QTest::keyClick(&dialog, Qt::Key_Return);
    QVERIFY(!dialog.isCanceled());
    QVERIFY(QFileInfo::exists(root.filePath("rewind/old/0002.clip")));
    QVERIFY(QFileInfo::exists(root.filePath("rewind/0002_r.clip.txt")));
    QVERIFY(!QFileInfo::exists(root.filePath("double-mapped")));
  }

  // Copy a directory with a kana drawing, Finder metadata and a user dotfile.
  // Undo must preserve a drawing added by another user after the copy.
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

  // The stored source and local destination name the same drawing. Refuse an
  // overwrite while allowing the two skip policies to finish safely.
  void mappedCopyPreservesSource() {
    const QString dir = root.filePath("alias-copy");
    QVERIFY(QDir().mkpath(dir));
    const QString source = dir + "/drawing.clip";
    QVERIFY(write(source, "drawing"));
    StepExecutor_CopyFolderContents copy(
        0,
        {Step_CopyFolderContents,
         {"[ALL_WITHOUT_UI][OVERWRITE]", stored + "/alias-copy", dir}},
        source, {}, "0001", false, true);
    QString log;
    QVariant record;
    QVERIFY(!copy.execute(log, record));
    QFile original(source);
    QVERIFY(original.open(QIODevice::ReadOnly));
    QCOMPARE(original.readAll(), "drawing");
    for (const QString mode : {"[SKIP_EXISTING]", "[OVERWRITE_IF_NEWER]"}) {
      StepExecutor_CopyFolderContents skip(
          0,
          {Step_CopyFolderContents,
           {"[ALL_WITHOUT_UI]" + mode, stored + "/alias-copy", dir}},
          source, {}, "0001", false, true);
      QVERIFY2(skip.execute(log, record), qPrintable(log));
    }
  }

  // An unmapped foreign path cannot supply local project data. Registration
  // must report the missing info mapping even when the cue root resolves.
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

    configureLayout();
    wm->setInfoFolderPath(foreign);
    QVERIFY(!wm->projectInfoMappingError().isEmpty());
    wm->setInfoFolderPath(stored + "/info");
    QVERIFY(wm->projectInfoMappingError().isEmpty());
  }

  // Move, Ask Delete and undoing Create Empty Folder all treat a folder
  // containing only hidden or system files as empty.
  void hiddenFolderCleanup_data() {
    QTest::addColumn<QString>("action");
    QTest::newRow("move source") << QString("move");
    QTest::newRow("ask delete") << QString("delete");
    QTest::newRow("undo create empty folder") << QString("undo");
  }

  // Leave only hidden entries in a scene directory. Each action removes it,
  // while undo keeps the folder if another artist added a visible drawing.
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
    QVERIFY(QDir().mkpath(dir + "/.hidden"));
    QVERIFY(write(dir + "/.hidden/content", "hidden subtree"));
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
      QVERIFY(!remove.execute(log, record));  // vanished since confirmation

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

  // Create a file between two lookups: only the next refresh should see
  // the newly created spelling in its directory snapshot.
  void refreshDirectorySnapshot() {
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

  // Use a decomposed scene prefix with existing kana filenames. Thumbnails,
  // logs and scene-info updates must refer to the existing files.
  void japaneseSceneFiles() {
    configureLayout();
    auto* wm = WorkflowManager::instance();
    const QString kana =
        QStringLiteral("ガ").normalized(QString::NormalizationForm_D);
    wm->setCutNumberPrefix(kana);
    wm->setSceneThumbnailPath("[ROOT]/[PREFIX][CUTNUMBER].png");
    QVERIFY(write(root.filePath("ガ0015.png"), "image"));
    const QString thumb =
        QDir(root.path()).entryList({"*0015.png"}, QDir::Files).value(0);
    QCOMPARE(wm->sceneThumbnailPathFor({}, "0015"), root.filePath(thumb));
    QDir logs(root.filePath("logs")), info(root.filePath("info/scene_info"));
    QVERIFY(logs.mkpath(".") && info.mkpath("."));
    QVERIFY(write(logs.filePath("ガ0015.md"), "first line\n"));
    QVERIFY(write(info.filePath("ガ0015.ini"), "[General]\nNote=previous\n"));
    wm->logAction({}, "0015", "layout", "review");
    QCOMPARE(logs.entryList({"*0015.md"}, QDir::Files).size(), 1);
    QCOMPARE(wm->logFilePath({}, "0015"),
             logs.filePath(logs.entryList({"*0015.md"}, QDir::Files).first()));
    QCOMPARE(wm->sceneInfoTag({}, "0015", "Note", {}).toString(), "previous");
    wm->saveSceneInfoTag({}, "0015", "Note", "updated");
    QCOMPARE(info.entryList({"*0015.ini"}, QDir::Files).size(), 1);
    QCOMPARE(wm->sceneInfoTags({}, "0015").value("Note").toString(), "updated");
  }

  // Look up a decomposed filename by its composed spelling. On volumes that
  // can hold both names, a collision must leave both files untouched.
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
    // Stuff-relative paths must use the on-disk spelling without remapping.
    const QString relative = existing.first().normalized(
        existing.first() ==
                existing.first().normalized(QString::NormalizationForm_C)
            ? QString::NormalizationForm_D
            : QString::NormalizationForm_C);
    if (WorkflowManager::resolvePathFromStuffFolder(relative) !=
        root.filePath(enabled ? existing.first() : relative))
      return 5;
    auto* wm = WorkflowManager::instance();
    wm->setInfoFolderPath("info");
    FlowGroupInfo group;
    group.xdtsFileName = QStringLiteral("ガ.lookup");
    wm->setFlowGroupInfos({{0, group}});
    QStringList names{kana + ".lookup"};
    if (wm->xdtsPaths(0, {}, "0001", names, false, {input}).size() !=
        (enabled ? 0 : 1))
      return 6;
    return 0;
  }
  SharedProjectCompatibilityTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "sharedprojectcompatibility_test.moc"
