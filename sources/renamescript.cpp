#include "renamescript.h"

#include "usersettings.h"

#include <QJSEngine>
#include <QJSValueIterator>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace {
QString g_language = "en";

// スクリプトの読み込み（評価）にかける時間制限
const int LoadTimeoutMs = 5000;

// エラーの値から、行番号つきのメッセージを作る
QString errorMessage(const QJSValue& error) {
  QString message = error.property("message").toString();
  if (message.isEmpty()) message = error.toString();
  QJSValue line = error.property("lineNumber");
  if (line.isNumber() && line.toInt() > 0)
    return QObject::tr("line %1: %2").arg(line.toInt()).arg(message);
  return message;
}

// 文字列、または言語ごとの文字列のオブジェクトから、現在の言語の文字列を取り出す。
// 現在の言語の文字列が無い場合は、最初に登録されている文字列を使う
QString localizedText(const QJSValue& value) {
  if (value.isString()) return value.toString();
  if (!value.isObject()) return QString();

  QJSValue current = value.property(g_language);
  if (current.isString() && !current.toString().isEmpty())
    return current.toString();

  QJSValueIterator it(value);
  while (it.hasNext()) {
    it.next();
    if (it.value().isString() && !it.value().toString().isEmpty())
      return it.value().toString();
  }
  return QString();
}

// スクリプトが返したパスを、コピー先フォルダの外に出ない相対パスに正規化する
bool normalizeRelativePath(QString path, QString& normalized) {
  path.replace('\\', '/');
  if (path.startsWith('/') || (path.size() >= 2 && path[1] == ':'))
    return false;  // 絶対パス
  QString cleaned = QDir::cleanPath(path);
  if (cleaned.isEmpty() || cleaned == "." || cleaned == ".." ||
      cleaned.startsWith("../"))
    return false;
  normalized = cleaned;
  return true;
}
}  // namespace

//-----------------------------------------------------------------------------------

// 一定時間が過ぎたらQJSEngineの実行を中断する見張り。
// デストラクタで見張りを止める（時間切れで中断した場合は、エンジンの中断状態も戻す）
class ScriptWatchdog {
public:
  ScriptWatchdog(QJSEngine* engine, int timeoutMs) : m_engine(engine) {
    m_thread = std::thread([this, timeoutMs]() {
      std::unique_lock<std::mutex> lock(m_mutex);
      if (!m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                         [this]() { return m_done; })) {
        m_timedOut = true;
        m_engine->setInterrupted(true);
      }
    });
  }
  ~ScriptWatchdog() {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_done = true;
    }
    m_cv.notify_all();
    m_thread.join();
    if (m_timedOut) m_engine->setInterrupted(false);
  }
  bool timedOut() const { return m_timedOut; }

private:
  QJSEngine* m_engine;
  std::thread m_thread;
  std::mutex m_mutex;
  std::condition_variable m_cv;
  bool m_done = false;
  std::atomic<bool> m_timedOut{false};
};

//-----------------------------------------------------------------------------------

namespace {
// スクリプトを読み込んで評価し、名前・説明とrename関数を取り出す。失敗した場合はinfo.errorに理由を入れる
void loadScript(QJSEngine& engine, const QString& filePath,
                RenameScript::Info& info, QJSValue& renameFunction) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    info.error = QObject::tr("The script file could not be opened.");
    return;
  }
  QString source = QString::fromUtf8(file.readAll());
  if (source.startsWith(QChar(0xFEFF))) source.remove(0, 1);  // BOM

  engine.installExtensions(QJSEngine::ConsoleExtension);

  bool timedOut = false;
  QJSValue result;
  {
    ScriptWatchdog watchdog(&engine, LoadTimeoutMs);
    result   = engine.evaluate(source, QFileInfo(filePath).fileName());
    timedOut = watchdog.timedOut();
  }
  if (timedOut) {
    info.error = QObject::tr("The script took too long to load.");
    return;
  }
  if (result.isError()) {
    info.error = errorMessage(result);
    return;
  }

  QJSValue rule = engine.globalObject().property("rule");
  if (rule.isObject()) {
    QString name = localizedText(rule.property("name"));
    if (!name.isEmpty()) info.name = name;
    info.description = localizedText(rule.property("description"));
  }

  renameFunction = engine.globalObject().property("rename");
  if (!renameFunction.isCallable())
    info.error =
        QObject::tr("The function rename(path, context) is not defined.");
}
}  // namespace

const QString RenameScript::LegacyGengaScriptId = "genga_to_uchikomi";

void RenameScript::setLanguage(const QString& language) {
  g_language = language;
}

QString RenameScript::language() { return g_language; }

QString RenameScript::scriptsFolderPath() {
  QString stuff = UserSettings::instance()->stuffFolderPath();
  if (stuff.isEmpty()) return QString();
  return stuff + "/rename_scripts";
}

namespace {
// スクリプトのファイルを探す。Stuffフォルダのrename_scriptsに無い場合は、同梱のサンプルを使う
QString findScriptFile(const QString& id) {
  if (id.isEmpty() || id.contains('/') || id.contains('\\')) return QString();
  QString folderPath = RenameScript::scriptsFolderPath();
  if (!folderPath.isEmpty()) {
    QString path = folderPath + "/" + id + ".js";
    if (QFileInfo(path).isFile()) return path;
  }
  QString bundledPath = ":Resources/rename_scripts/" + id + ".js";
  if (QFileInfo(bundledPath).isFile()) return bundledPath;
  return QString();
}
}  // namespace

void RenameScript::installBundledSamplesIfNeeded() {
  QString folderPath = scriptsFolderPath();
  if (folderPath.isEmpty() || QFileInfo::exists(folderPath)) return;
  if (!QDir().mkpath(folderPath)) return;

  // 同梱のサンプルスクリプトを置く（フォルダが無かったときだけ。ユーザーが消したものは復活させない）
  QDir samples(":Resources/rename_scripts");
  for (const QString& fileName : samples.entryList({"*.js"}, QDir::Files)) {
    QString destPath = folderPath + "/" + fileName;
    if (QFile::copy(samples.absoluteFilePath(fileName), destPath))
      QFile::setPermissions(destPath, QFile::permissions(destPath) |
                                          QFileDevice::WriteOwner |
                                          QFileDevice::WriteUser);
  }
}

QList<RenameScript::Info> RenameScript::availableScripts() {
  installBundledSamplesIfNeeded();

  QList<Info> list;
  QString folderPath = scriptsFolderPath();
  if (folderPath.isEmpty()) return list;
  for (const QString& fileName :
       QDir(folderPath)
           .entryList({"*.js"}, QDir::Files, QDir::Name | QDir::IgnoreCase)) {
    Info info;
    info.id       = QFileInfo(fileName).completeBaseName();
    info.filePath = folderPath + "/" + fileName;
    info.name     = info.id;
    QJSEngine engine;
    QJSValue renameFunction;
    loadScript(engine, info.filePath, info, renameFunction);
    list.append(info);
  }
  return list;
}

RenameScript::Info RenameScript::scriptInfo(const QString& id) {
  for (const Info& info : availableScripts())
    if (info.id == id) return info;
  // Stuffフォルダに無くても、同梱のスクリプトならそれを使う
  QString bundledPath = findScriptFile(id);
  if (!bundledPath.isEmpty()) {
    Info info;
    info.id       = id;
    info.filePath = bundledPath;
    info.name     = id;
    QJSEngine engine;
    QJSValue renameFunction;
    loadScript(engine, info.filePath, info, renameFunction);
    return info;
  }
  Info missing;
  missing.id   = id;
  missing.name = id;
  missing.error =
      QObject::tr(
          "The script \"%1\" was not found in the rename_scripts folder.")
          .arg(id);
  return missing;
}

RenameScript::RenameScript(const QString& id, const QVariantMap& context) {
  installBundledSamplesIfNeeded();

  m_info.id       = id;
  m_info.name     = id;
  m_info.filePath = findScriptFile(id);
  if (m_info.filePath.isEmpty()) {
    m_info.error =
        QObject::tr(
            "The script \"%1\" was not found in the rename_scripts folder.")
            .arg(id);
    return;
  }

  m_engine = new QJSEngine();
  loadScript(*m_engine, m_info.filePath, m_info, m_renameFunction);
  if (m_info.isValid()) m_context = m_engine->toScriptValue(context);
}

RenameScript::~RenameScript() {
  m_watchdog.reset();
  delete m_engine;
}

void RenameScript::beginBatch(int timeoutMs) {
  if (m_engine) m_watchdog.reset(new ScriptWatchdog(m_engine, timeoutMs));
}

void RenameScript::endBatch() { m_watchdog.reset(); }

bool RenameScript::rename(const QString& relativePath, QString& result,
                          QString& error) {
  result.clear();
  if (!m_info.isValid()) {
    error = m_info.error;
    return false;
  }
  if (m_watchdog && m_watchdog->timedOut()) {
    error = QObject::tr("The script took too long to run.");
    return false;
  }

  QJSValue returned =
      m_renameFunction.call(QJSValueList{QJSValue(relativePath), m_context});
  if (m_watchdog && m_watchdog->timedOut()) {
    error = QObject::tr("The script took too long to run.");
    return false;
  }
  if (returned.isError()) {
    error = errorMessage(returned);
    return false;
  }
  // 値を返さない場合は、対象外として扱う
  if (returned.isUndefined() || returned.isNull()) return true;
  if (!returned.isString()) {
    error = QObject::tr("The function rename() must return a string.");
    return false;
  }

  QString path = returned.toString();
  if (path.isEmpty()) return true;  // リネームの対象外
  if (!normalizeRelativePath(path, result)) {
    result.clear();
    error = QObject::tr("The function rename() returned an invalid path: %1")
                .arg(path);
    return false;
  }
  return true;
}
