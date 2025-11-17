#pragma once
#include <QTimer>
#include <QSocketDescriptor>

#include "curl/curl.h"

class SocketNotifiers {
public:
    QSocketNotifier* read_notifier = nullptr;
    QSocketNotifier* write_notifier = nullptr;
};

class QCurlEasy : public QObject {
    Q_OBJECT
    friend class QCurl;
public:

    explicit QCurlEasy(CURL *curl, QObject* parent = nullptr);
    ~QCurlEasy() override;
signals:
    void done(long code, QByteArray body);

private:
    void emit_done();

public:
    CURL *_curl;

private:
    QByteArray data = QByteArray();

};

class QCurl : public QObject {
    Q_OBJECT
    friend class QCurlEasy;

public:
    explicit QCurl(QObject *parent = nullptr);
    ~QCurl() override;

private:
    static int timer_callback(CURLM *multi, long timeout_ms, QCurl *qcurl);
    // libcurl 有新增/删除的 socket -> 通知 qt
    static int socket_callback(CURL *easy, curl_socket_t s, int what, QCurl *qcurl, SocketNotifiers *notifiers);

    // qt 监听到 socket 上有事件发生 -> 通知 libcurl
    void handleSocketAction(CURL *easy, curl_socket_t s, int mask);

private:
    QTimer *_timer;
    CURLM *_curlm;

    // 避免在 QApplication 之前创建
    static QCurl &instance();

};