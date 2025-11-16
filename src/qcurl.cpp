#include "qcurl.h"
#include <QSocketNotifier>
#include <utility>

int curl_init_code = curl_global_init(CURL_GLOBAL_ALL);

QCurlEasy::QCurlEasy(CURL *curl) : _curl(curl) {
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
}


void QCurl::emit_done(long code, QByteArray body) {
    emit easy_done(code, std::move(body));
}


int QCurl::timer_callback(CURLM *multi, long timeout_ms, QCurl *qcurl) {
    if (timeout_ms < 0) {
        qcurl->_timer->stop();
    } else {
        qcurl->_timer->start((int)timeout_ms);
    }
    return 0;
}

int QCurl::socket_callback(CURL *easy, curl_socket_t s, int what, QCurl *qcurl, void *socketp) {
    QCurlEasy *qcurl_easy;
    if (qcurl->_curls.contains(easy)) {
        qcurl_easy = qcurl->_curls.at(easy);
    } else {
        qcurl_easy = new QCurlEasy(easy);
        qcurl->_curls[easy] = qcurl_easy;
    }
    // 此 socket 初次出现
    if ((what == CURL_POLL_IN || what == CURL_POLL_INOUT) && !qcurl->_read_socket_notifiers.contains(s)) {
        QSocketNotifier *notifier;
        notifier = new QSocketNotifier(static_cast<qintptr>(s), QSocketNotifier::Read, qcurl);
        qcurl->_read_socket_notifiers[s] = notifier;
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION,
                         [](const char *ptr, size_t size, size_t nmemb, QCurlEasy* qcurl_easy) {
                         qcurl_easy->data.append(ptr, size * nmemb);
                         });
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, &qcurl_easy);
        connect(notifier, &QSocketNotifier::activated, qcurl, [qcurl, s, qcurl_easy]() {
            curl_multi_socket_action(qcurl->_curlm, s, 0, nullptr);
            int n_msgs;
            do {
                const CURLMsg *msg = curl_multi_info_read(qcurl->_curlm, &n_msgs);
                if (msg == nullptr) {
                    break;
                }
                if (msg->msg == CURLMSG_DONE) {
                    qcurl_easy->emit_done();
                }
            } while (true);
        });
    }
    if (what == CURL_POLL_OUT || what == CURL_POLL_INOUT) {
        assert(false);
    }
    if (what == CURL_POLL_REMOVE) {
        if (qcurl->_write_socket_notifiers.contains(s)) {
            delete qcurl->_write_socket_notifiers[s];
            qcurl->_write_socket_notifiers.erase(s);
        }
        if (qcurl->_curls.contains(easy)) {
            delete qcurl->_curls[easy];
            qcurl->_curls.erase(easy);
        }
    }
}