#pragma once
#ifndef GOOGLECHATGATEWAY_H
#define GOOGLECHATGATEWAY_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QQueue>
#include <functional>

class QNetworkAccessManager;
class QTimer;

// Google Chatのスペースへメッセージを送る。
// Chatの投稿先のWebhook
// URLは、投稿権限を持つ秘密情報のため、PPM（クライアント）には持たせない。
// PPMは、プロジェクト設定のGoogle Apps
// ScriptのURLへ、投稿先の名前と本文だけを送り、 名前に対応するWebhook
// URLを持つGAS（スクリプト プロパティに登録）が、Chatへ投稿する。
//
// 送信は非同期（メインスレッドのイベントループ上）で行い、呼び出し元をブロックしない。
// Chatの投稿頻度の上限（スペースあたり毎秒1件程度）を超えないよう、全リクエストを
// ひとつのキューに入れて一定間隔で順番に送り、一時的なエラーは数回まで再試行する
class GoogleChatGateway : public QObject {
  Q_OBJECT
public:
  // ok=falseのとき、errorには（URLを含まない）失敗理由が入る
  typedef std::function<void(bool ok, const QString& error)> ResultCallback;

  static GoogleChatGateway* instance();

  // gasUrlは、Google Apps
  // ScriptのウェブアプリのURL。destinationNameは、GAS側に登録された投稿先の名前。
  // threadKeyが空でなければ、同じキーのメッセージを同一スレッドにまとめる。
  // callbackを省略した場合、送信に失敗したときは警告ダイアログを表示する
  void post(const QString& gasUrl, const QString& destinationName,
            const QString& text, const QString& threadKey = QString(),
            ResultCallback callback = nullptr);

  // GASに登録されている投稿先の名前の一覧を取得する（プロジェクト設定で、投稿先の名前を
  // 手で入力する手間を省くため）。URLは取得できない。送信キューを通さず、すぐにリクエストを送る。
  // ok=falseのとき、errorには（URLを含まない）失敗理由が入る
  typedef std::function<void(bool ok, const QStringList& names,
                             const QString& error)>
      DestinationsCallback;
  void fetchDestinations(const QString& gasUrl, DestinationsCallback callback);

  // 送信待ち・送信中（最終的な成功／失敗がまだ確定していない）メッセージの件数。
  // アプリ終了時に未送信のメッセージが失われないかの確認に使う
  int pendingCount() const { return m_pendingCount; }

private:
  struct Job {
    QString gasUrl;
    QString destinationName;
    QString text;
    QString threadKey;
    ResultCallback callback;
    int attempt = 0;
  };

  explicit GoogleChatGateway(QObject* parent = nullptr);

  void startNext();
  void sendJob(Job job);
  void finishJob(const Job& job, bool ok, const QString& error);

  QNetworkAccessManager* m_networkManager;
  QQueue<Job> m_queue;
  bool m_busy        = false;
  int m_pendingCount = 0;
};

#endif  // GOOGLECHATGATEWAY_H
