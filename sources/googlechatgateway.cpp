#include "googlechatgateway.h"

#include "workflow.h"

#include <QApplication>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QTextStream>
#include <QStringConverter>
#include <QRegularExpression>

namespace {
// Chatへの連続投稿の最小間隔。Webhookの投稿頻度の上限（スペースあたり毎秒1件程度）を
// 超えてエラーにならないよう、1件送るたびにこれだけ空けてから次を送る
const int SendIntervalMs    = 1100;
const int MaxAttempts       = 3;
const int TransferTimeoutMs = 30000;

// 全リクエストの履歴（投稿先名、成功／失敗、失敗理由）を追記するログ。
// GASのURLは、Chatへ投稿できる入口なので、ログには残さない
void appendChatLog(const QString& line) {
  QString logRoot = WorkflowManager::instance()->logRootPath();
  if (logRoot.isEmpty()) return;
  QDir dir(logRoot + "/google_chat");
  if (!dir.exists()) dir.mkpath(".");
  QFile file(dir.absoluteFilePath("google_chat.log"));
  if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    return;
  QTextStream out(&file);
  out.setEncoding(QStringConverter::Utf8);
  out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " "
      << line << "\n";
}

// QNetworkReply::errorString()等はURL（＝GASのURL）を含むため、表示・記録の前に取り除く
QString scrubUrls(const QString& text) {
  QString s = text;
  s.replace(QRegularExpression("https?://\\S+"), "<URL>");
  return s;
}

// GASが返したエラーの文には、原因の調査に必要なURL（例：不足している権限のURL）が含まれうるため、
// 全てのURLを消すのではなく、GASのURL自体だけを取り除く
QString scrubSecret(const QString& text, const QString& secretUrl) {
  QString s = text;
  if (!secretUrl.trimmed().isEmpty()) s.replace(secretUrl.trimmed(), "<URL>");
  return s;
}

// 接続できなかった（リクエストがGASに届いていない）ことが確実なエラー。
// タイムアウトのように、投稿済みかもしれないエラーは、再試行すると二重投稿になりうるため含めない
bool isConnectionFailure(QNetworkReply::NetworkError error) {
  return error == QNetworkReply::ConnectionRefusedError ||
         error == QNetworkReply::HostNotFoundError ||
         error == QNetworkReply::TemporaryNetworkFailureError ||
         error == QNetworkReply::NetworkSessionFailedError;
}
}  // namespace

//-----------------------------------------------------------------------------------

GoogleChatGateway* GoogleChatGateway::instance() {
  // QCoreApplicationを親にして、アプリ終了時にQNetworkAccessManagerごと確実に破棄させる
  static GoogleChatGateway* _instance = new GoogleChatGateway(qApp);
  return _instance;
}

GoogleChatGateway::GoogleChatGateway(QObject* parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)) {}

void GoogleChatGateway::post(const QString& gasUrl,
                             const QString& destinationName,
                             const QString& text, const QString& threadKey,
                             ResultCallback callback) {
  Job job;
  job.gasUrl          = gasUrl;
  job.destinationName = destinationName;
  job.text            = text;
  job.threadKey       = threadKey;
  job.callback        = callback;

  ++m_pendingCount;
  appendChatLog(QString("[QUEUED] dest=%1 thread=%2 length=%3")
                    .arg(destinationName, threadKey)
                    .arg(text.size()));
  m_queue.enqueue(job);
  if (!m_busy) startNext();
}

void GoogleChatGateway::fetchDestinations(const QString& gasUrl,
                                          DestinationsCallback callback) {
  QUrl url(gasUrl);
  if (gasUrl.trimmed().isEmpty() || !url.isValid() || url.scheme().isEmpty()) {
    callback(false, QStringList(),
             tr("The Google Apps Script URL is not set (or invalid) in the "
                "project settings."));
    return;
  }

  QJsonObject body;
  body.insert("action", "listChatDestinations");

  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
  request.setTransferTimeout(TransferTimeoutMs);

  QNetworkReply* reply = m_networkManager->post(
      request, QJsonDocument(body).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [reply, callback, gasUrl]() {
    reply->deleteLater();
    int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray responseData = reply->readAll();

    if (reply->error() != QNetworkReply::NoError || httpStatus != 200) {
      callback(false, QStringList(),
               QString("HTTP status=%1, %2")
                   .arg(httpStatus)
                   .arg(scrubUrls(reply->errorString())));
      return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (!doc.isObject()) {
      callback(
          false, QStringList(),
          tr("Unexpected (non-JSON) response from the Google Apps Script. HTTP "
             "status=%1, body(first 200 chars)=%2")
              .arg(httpStatus)
              .arg(scrubUrls(
                  QString::fromUtf8(responseData.left(200)).simplified())));
      return;
    }
    QJsonObject obj = doc.object();
    if (!obj.value("success").toBool()) {
      callback(false, QStringList(),
               scrubSecret(obj.value("error").toString(), gasUrl));
      return;
    }
    // 古いバージョンのCode.gsは"destinations"を返さない（投稿先の一覧を取得する機能が無い）
    if (!obj.value("destinations").isArray()) {
      callback(false, QStringList(),
               tr("The Google Apps Script does not support getting the "
                  "destinations. Update it to the latest Code.gs."));
      return;
    }
    QStringList names;
    for (const QJsonValue& v : obj.value("destinations").toArray())
      if (v.isString() && !v.toString().isEmpty()) names.append(v.toString());
    callback(true, names, QString());
  });
}

void GoogleChatGateway::startNext() {
  if (m_queue.isEmpty()) {
    m_busy = false;
    return;
  }
  m_busy = true;
  sendJob(m_queue.dequeue());
}

void GoogleChatGateway::sendJob(Job job) {
  job.attempt++;

  QUrl url(job.gasUrl);
  if (job.gasUrl.trimmed().isEmpty() || !url.isValid() ||
      url.scheme().isEmpty()) {
    finishJob(job, false,
              tr("The Google Apps Script URL is not set (or invalid) in the "
                 "project settings."));
    return;
  }

  QJsonObject body;
  body.insert("action", "notifyChat");
  body.insert("destination", job.destinationName);
  body.insert("text", job.text);
  body.insert("threadKey", job.threadKey);

  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  // GASのウェブアプリはリダイレクト(302)を返すため、リダイレクト追従属性を有効化する
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  // GASのリダイレクト先への切り替え時、QtのHTTP/2実装がコネクションを誤って再利用し
  // "Host requires authentication"
  // で失敗することがあるため(QTBUG-97522)、HTTP/1.1に固定する
  request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
  request.setTransferTimeout(TransferTimeoutMs);

  QNetworkReply* reply = m_networkManager->post(
      request, QJsonDocument(body).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [this, reply, job]() {
    reply->deleteLater();

    int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray responseData = reply->readAll();

    QString error;
    bool retryable = false;
    if (reply->error() == QNetworkReply::NoError && httpStatus == 200) {
      QJsonDocument doc = QJsonDocument::fromJson(responseData);
      if (doc.isObject() && doc.object().value("success").toBool()) {
        finishJob(job, true, QString());
        return;
      }
      if (doc.isObject()) {
        // GAS側のエラー（投稿先が未登録、投稿数の上限、Chatからのエラー等）
        error = scrubSecret(doc.object().value("error").toString(), job.gasUrl);
        retryable = doc.object().value("retryable").toBool();
      } else {
        // GASがJSONではなくHTML等（エラーページ、ログイン要求など）を返した場合
        error =
            tr("Unexpected (non-JSON) response from the Google Apps Script. "
               "HTTP status=%1, body(first 200 chars)=%2")
                .arg(httpStatus)
                .arg(scrubUrls(
                    QString::fromUtf8(responseData.left(200)).simplified()));
      }
    } else {
      error = QString("HTTP status=%1, %2 (QNetworkReply::NetworkError=%3)")
                  .arg(httpStatus)
                  .arg(scrubUrls(reply->errorString()))
                  .arg((int)reply->error());
      // HTTP 429/5xxと、接続できなかった場合は再試行する
      retryable = (httpStatus == 429 || httpStatus >= 500 ||
                   isConnectionFailure(reply->error()));
    }

    if (retryable && job.attempt < MaxAttempts) {
      appendChatLog(QString("[RETRY] dest=%1 attempt=%2 %3")
                        .arg(job.destinationName)
                        .arg(job.attempt)
                        .arg(error));
      QTimer::singleShot(2000 * job.attempt, this,
                         [this, job]() { sendJob(job); });
      return;
    }
    finishJob(job, false, error);
  });
}

void GoogleChatGateway::finishJob(const Job& job, bool ok,
                                  const QString& error) {
  --m_pendingCount;
  if (ok)
    appendChatLog(QString("[OK] dest=%1 attempts=%2")
                      .arg(job.destinationName)
                      .arg(job.attempt));
  else
    appendChatLog(QString("[FAILED] dest=%1 attempts=%2 error=%3")
                      .arg(job.destinationName)
                      .arg(job.attempt)
                      .arg(error));

  // 次のメッセージは、投稿頻度の上限を超えないよう間隔を空けてから送る
  QTimer::singleShot(SendIntervalMs, this, &GoogleChatGateway::startNext);

  if (job.callback)
    job.callback(ok, error);
  else if (!ok) {
    QMessageBox::warning(QApplication::activeWindow(), tr("Warning"),
                         tr("Failed to send the Google Chat notification "
                            "(destination: %1).\n\n%2")
                             .arg(job.destinationName, error));
  }
}
