#pragma once
#ifndef RENAMESCRIPT_H
#define RENAMESCRIPT_H

#include <QString>
#include <QList>
#include <QVariantMap>
#include <QJSValue>
#include <memory>

class QJSEngine;
class ScriptWatchdog;

// リネームルールのスクリプト（JavaScript）。
//
// スクリプトはStuffフォルダ内の rename_scripts
// サブフォルダに置く（拡張子は.js）。 スクリプトは次の2つを定義する。
//
//   //
//   リネームルールの名前と説明。それぞれ、文字列か、言語ごとの文字列のオブジェクト。
//   // 現在の言語の文字列が無い場合は、最初に登録されている文字列が使われる。
//   var rule = {
//     name:        { en: "Rule name",        ja: "ルール名" },
//     description: { en: "Rule description", ja: "ルールの説明" }
//   };
//
//   //
//   コピー元のファイルのパス（選択元フォルダからの相対パス。サブフォルダに入っている場合はそのフォルダを含む）を受け取り、
//   //
//   コピー先のパス（サブフォルダに入れる場合はそのフォルダを含む相対パス）を返す。
//   // リネームの対象外（条件に合わない等）の場合は空の文字列を返す。
//   // contextには、sceneNumber / sequence / episode が入る（使わなくてよい）。
//   function rename(path, context) { ... }
//
// スクリプトはサンドボックス内で実行され、ファイルやネットワークにはアクセスできない。
//
// アプリに同梱のサンプルスクリプト（Resources/rename_scripts）は、Stuffフォルダのrename_scriptsに同名の
// ファイルが無い場合も、そのidを指定すれば、同梱のものが使われる（既存のプロジェクトを動かし続けるため）。
class RenameScript {
public:
  struct Info {
    QString
        id;  // ファイル名から拡張子を除いたもの。Modeの[RENAME_SCRIPT=<id>]で参照される
    QString filePath;
    QString name;         // 現在の言語での表示名（無ければid）
    QString description;  // 現在の言語での説明（無ければ空）
    QString error;        // 読み込みに失敗した場合、その理由
    bool isValid() const { return error.isEmpty(); }
  };

  // 従来、組み込みのリネームルールだった原画の打ち込み用リネーム（[RENAME_GENGA_TO_UCHIKOMI]）を
  // 移行した、同梱のスクリプトのid
  static const QString LegacyGengaScriptId;

  // スクリプトの名前・説明に使う言語（"en"、"ja"等）。既定は"en"
  static void setLanguage(const QString& language);
  static QString language();

  // Stuffフォルダ内のrename_scriptsフォルダ。Stuffフォルダが設定されていない場合は空文字列
  static QString scriptsFolderPath();
  // rename_scriptsフォルダがまだ無い場合に、フォルダを作って同梱のサンプルスクリプトを置く
  static void installBundledSamplesIfNeeded();
  // 利用可能なスクリプトの一覧（名前と説明のみを読み込む。リネーム関数は実行しない）
  static QList<Info> availableScripts();
  static Info scriptInfo(const QString& id);

  // contextは、rename()の第2引数に渡される
  RenameScript(const QString& id, const QVariantMap& context);
  ~RenameScript();

  bool isValid() const { return m_info.isValid(); }
  const Info& info() const { return m_info; }

  // 多数のファイルを続けてリネームする間、暴走したスクリプトを止めるための時間制限を設ける。
  // 制限時間を過ぎると、以降のrename()は失敗する
  void beginBatch(int timeoutMs);
  void endBatch();

  // 戻り値がfalseの場合はエラー（errorにメッセージ）。resultが空文字列なら、リネームの対象外
  bool rename(const QString& relativePath, QString& result, QString& error);

private:
  Info m_info;
  QJSEngine* m_engine = nullptr;
  QJSValue m_renameFunction;
  QJSValue m_context;
  std::unique_ptr<ScriptWatchdog> m_watchdog;
};

#endif  // RENAMESCRIPT_H
