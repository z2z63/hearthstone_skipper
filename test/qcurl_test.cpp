#include <QApplication>
#include <QDebug>
#include "qcurl.h"

int main(int argc, char *argv[]) {
    // 初始化 Qt 应用
    QCoreApplication app(argc, argv);

    qDebug() << "=== QCurl Test Started ===";
    qDebug() << "Making request to google.com...";

    CURL *curl = curl_easy_init();

    // 设置 URL
    curl_easy_setopt(curl, CURLOPT_URL, "https://google.com");

    // 创建 QCurlEasy 对象，这会自动添加到 multi handle
    QCurlEasy qcurl_easy =  QCurlEasy(curl);


    // 连接完成信号，打印响应数据
    QObject::connect(&qcurl_easy, &QCurlEasy::done, [&app](long code, QByteArray body) {
        qDebug() << "=== Request Completed ===";
        qDebug() << "HTTP Response Code:" << code;

        // 打印响应体的前 500 个字符
        QString bodyStr = QString::fromUtf8(body);
        qDebug().noquote() << bodyStr;

        qDebug() << "=== Test Finished Successfully ===";
        // 退出应用
        app.quit();
    });

    qDebug() << "Request created, waiting for response...";

    // 进入 Qt 事件循环，等待异步响应
    return QCoreApplication::exec();
}


