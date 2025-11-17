#include "qcurl.h"
#include <QSocketNotifier>
#include <utility>
#include <iostream>
#include <QDebug>

int curl_init_code = curl_global_init(CURL_GLOBAL_ALL);

QCurl &QCurl::instance() {
    static QCurl inst;
    return inst;
}

QCurlEasy::QCurlEasy(CURL *curl, QObject *parent) : QObject(parent), _curl(curl) {
    curl_easy_setopt(curl, CURLOPT_PRIVATE, this);
    curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION,
                     +[](const char *ptr, size_t size, size_t nmemb, QCurlEasy* qcurl_easy) -> size_t {
                         qcurl_easy->data.append(ptr, (int)(size * nmemb));
                         return size * nmemb;
                     });
    curl_easy_setopt(_curl, CURLOPT_WRITEDATA, this);
    curl_multi_add_handle(QCurl::instance()._curlm, _curl);
}

QCurlEasy::~QCurlEasy() {
    curl_easy_cleanup(_curl);
    data.clear();
}


void QCurlEasy::emit_done() {
    long code;
    curl_easy_getinfo(_curl, CURLINFO_RESPONSE_CODE, &code);
    emit done(code, data);
}


QCurl::QCurl(QObject *parent) : QObject(parent), _timer(new QTimer(this)) {
    _curlm = curl_multi_init();
    curl_multi_setopt(_curlm, CURLMOPT_SOCKETFUNCTION, socket_callback);
    curl_multi_setopt(_curlm, CURLMOPT_SOCKETDATA, this);
    curl_multi_setopt(_curlm, CURLMOPT_TIMERFUNCTION, timer_callback);
    curl_multi_setopt(_curlm, CURLMOPT_TIMERDATA, this);

    // 连接 timer 到 socket_action
    connect(_timer, &QTimer::timeout, this, [this] {
        int running_handles;
        curl_multi_socket_action(_curlm, CURL_SOCKET_TIMEOUT, 0, &running_handles);
    });
}

// Extracted from duplicated lambda bodies to centralize action & completion dispatch
void QCurl::handleSocketAction(CURL *easy, curl_socket_t s, int mask) {
    int running_handles;
    curl_multi_socket_action(_curlm, s, mask, &running_handles);
    int n_msgs;
    while (true) {
        const CURLMsg *msg = curl_multi_info_read(_curlm, &n_msgs);
        if (msg == nullptr) {
            break;
        }
        if (msg->msg == CURLMSG_DONE) {
            QCurlEasy *qeasy = nullptr;
            // Use easy from callback, safer than CURLINFO_PRIVATE with potential different pointer
            curl_easy_getinfo(easy, CURLINFO_PRIVATE, &qeasy);
            if (qeasy) {
                qeasy->emit_done();
            }
        }
    }
}

int QCurl::timer_callback(CURLM *multi, long timeout_ms, QCurl *qcurl) {
    if (timeout_ms < 0) {
        qcurl->_timer->stop();
    } else {
        qcurl->_timer->start((int)timeout_ms);
    }
    return 0;
}

int QCurl::socket_callback(CURL *easy, curl_socket_t s, int what, QCurl *qcurl, SocketNotifiers *notifiers) {
    // 此 socket 初次出现，加入 qt 事件循环中监听 socket 事件
    if ((what == CURL_POLL_IN || what == CURL_POLL_INOUT) && (
            notifiers == nullptr || notifiers->read_notifier == nullptr)) {
        if (notifiers == nullptr) {
            notifiers = new SocketNotifiers();
            curl_multi_assign(qcurl->_curlm, s, notifiers);
        }
        notifiers->read_notifier = new QSocketNotifier(static_cast<qintptr>(s), QSocketNotifier::Read, qcurl);

        // socket 事件发生，通知 libcurl
        connect(notifiers->read_notifier, &QSocketNotifier::activated, qcurl, [easy,qcurl, s]() {
            qcurl->handleSocketAction(easy, s, CURL_CSELECT_IN);
        });
    }
    if ((what == CURL_POLL_OUT || what == CURL_POLL_INOUT) && (
            notifiers == nullptr || notifiers->write_notifier == nullptr)) {
        if (notifiers == nullptr) {
            notifiers = new SocketNotifiers();
            curl_multi_assign(qcurl->_curlm, s, notifiers);
        }
        notifiers->write_notifier = new QSocketNotifier(static_cast<qintptr>(s), QSocketNotifier::Write, qcurl);

        connect(notifiers->write_notifier, &QSocketNotifier::activated, qcurl, [easy,qcurl, s]() {
            qcurl->handleSocketAction(easy, s, CURL_CSELECT_OUT);
        });
    }
    if (what == CURL_POLL_IN && notifiers->write_notifier != nullptr) {
        notifiers->write_notifier->setEnabled(false);
    }
    if (what == CURL_POLL_OUT && notifiers->read_notifier != nullptr) {
        notifiers->read_notifier->setEnabled(true);
    }
    if (what == CURL_POLL_REMOVE && notifiers != nullptr) {
        curl_multi_assign(qcurl->_curlm, s, nullptr);
        delete notifiers->read_notifier;
        delete notifiers->write_notifier;
        delete notifiers;
    }
    return 0;
}

QCurl::~QCurl() {
    curl_multi_cleanup(_curlm);
}