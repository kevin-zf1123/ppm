#pragma once
#ifndef GOOGLEGATEWAY_H
#define GOOGLEGATEWAY_H

#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;

// Google Apps Script(GAS)のウェブアプリへHTTP
// POSTし、スプレッドシートへの書き込みを行う
class GoogleWorker : public QObject {
  Q_OBJECT

  QString m_gasUrl;
  QString m_sheetName;
  QStringList m_cutNumbers;
  QString m_flowName;
  QStringList m_values;
  // 呼び出し元(StepExecutor_InputGoogleSpreadsheet)が、ひとつのアクション内で複数の
  // Googleスプレッドシート入力ステップを区別するために割り当てたID。GoogleGateway/GoogleWorker
  // 自体はこの値の意味を解釈せず、完了・失敗シグナルでそのまま呼び出し元へ返すだけ
  int m_requestId;
  QNetworkAccessManager* m_networkManager;

public:
  GoogleWorker(QString gasUrl, QString sheetName, QStringList cutNumbers,
               QString flowName, QStringList values, int requestId);

public slots:
  void process();
signals:
  void inputDone(QString, QStringList, int);
  void inputFailed(QString, int);
  void processEnd();
};

class GoogleGateway : public QObject {
  Q_OBJECT
public:
  static GoogleGateway* instance();

  // requestIdは、呼び出し元がひとつのアクション内で複数のGoogleスプレッドシート入力ステップを
  // 区別するための任意のID。googleSpreadSheetInput/googleSpreadSheetInputFailedシグナルで
  // そのまま返される
  bool updateSingleCell(QString sheetName, QStringList cutNumbers,
                        QString flowName, QStringList values,
                        QStringList& prevValues, int requestId);

  // GASのURLが設定されている場合のみ有効
  bool isActive() const;

  // 送信待ち・送信中（完了/失敗の応答をまだ受け取っていない）書き込みリクエストの件数。
  // メインスレッドからのみ参照すること。アプリ終了時に書き込み未完了でないかの確認に使う
  int pendingCount() const { return m_pendingCount; }

private:
  explicit GoogleGateway(QObject* parent = nullptr);
  int m_pendingCount = 0;
signals:
  void googleSpreadSheetInput(QString, QStringList, int);
  void googleSpreadSheetInputFailed(QString, int);
};

#endif  // GOOGLEGATEWAY_H
