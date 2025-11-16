#pragma once
#include <QTimer>
#include <QSocketDescriptor>
#include <QJsonDocument>

#include "curl/curl.h"

class QCurlEasy : QObject {
    friend class QCurl;


    QCurlEasy(CURL *curl);
    ~QCurlEasy();
signals:
    void done(long code, QByteArray body);

private:
    void emit_done();

public:
    CURL *_curl;

private:
    QByteArray data = QByteArray();

};

class QCurl : QObject {
public:
    QCurl(QObject *parent = nullptr);
    ~QCurl();

public slots:
    void easy_done(long code, QByteArray body);

private:
    static int timer_callback(CURLM *multi, long timeout_ms, QCurl *qcurl);
    static int socket_callback(CURL *easy, curl_socket_t s, int what, QCurl *qcurl, void *socketp);

    void emit_done(long code, QByteArray body);

    int setTimeout(long timeout_ms);

private:
    QTimer *_timer;
    CURLM *_curlm;
    std::map<CURL *, QCurlEasy *> _curls;
    std::map<curl_socket_t, QSocketNotifier*> _read_socket_notifiers;
    std::map<curl_socket_t, QSocketNotifier*> _write_socket_notifiers;

};